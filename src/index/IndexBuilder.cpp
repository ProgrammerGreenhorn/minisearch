#include "minisearch/index/IndexBuilder.hpp"

#include <algorithm>
#include <exception>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minisearch/index/FileScanner.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/index/TextParser.hpp"
#include "minisearch/util/Logger.hpp"
#include "minisearch/util/ThreadPool.hpp"

namespace minisearch::index {

namespace {

using util::ThreadPool;

auto recordKey(const std::filesystem::path& path) -> std::string {
  return path.lexically_normal().string();
}

auto sameIndexedFile(const FileRecord& current,
                     const FileRecord& previous) -> bool {
  return current.size == previous.size &&
         current.textIndexed == previous.textIndexed &&
         current.contentHash == previous.contentHash;
}

auto loadPreviousIndex(const std::filesystem::path& indexFile,
                       const IndexStorage& storage)
    -> std::optional<InvertedIndex> {
  std::error_code error;
  const bool exists = std::filesystem::exists(indexFile, error);
  if (error) {
    MINISEARCH_LOG_WARNING("failed to check existing index file: " +
                           indexFile.string());
    return std::nullopt;
  }

  if (!exists) {
    return std::nullopt;
  }

  try {
    InvertedIndex index = storage.load(indexFile);
    MINISEARCH_LOG_INFO("loaded existing index: " + indexFile.string());
    return index;
  } catch (const std::exception& error) {
    MINISEARCH_LOG_WARNING("failed to load existing index; rebuilding: " +
                           std::string(error.what()));
    return std::nullopt;
  }
}

auto previousRecordsByPath(const InvertedIndex& index)
    -> std::unordered_map<std::string, FileRecord> {
  std::unordered_map<std::string, FileRecord> records;
  records.reserve(index.records().size());
  for (const auto& record : index.records()) {
    records.emplace(recordKey(record.path), record);
  }
  return records;
}

auto parseChangedTextFiles(const InvertedIndex& index,
                           const std::vector<InvertedIndex::DocumentId>& ids,
                           std::size_t threads)
    -> std::vector<std::vector<std::string>> {
  // map from document ID to terms contained in the corresponding text file
  // the file changed are reparsed in parallel, while unchanged files reuse terms from the previous index
  std::vector<std::vector<std::string>> termsByDocument(index.fileCount());
  TextParser parser;
  ThreadPool pool(threads);

  using ParseResult =
      std::pair<InvertedIndex::DocumentId, std::vector<std::string>>;
  std::vector<std::future<ParseResult>> futures;

  for (const auto id : ids) {
    const FileRecord& record = index.records()[id];
    MINISEARCH_LOG_INFO("parsing text file: " + record.path.string());
    futures.push_back(pool.submit([record, &parser]() -> ParseResult {
      return {record.id, parser.parseFile(record.path)};
    }));
  }

  for (auto& future : futures) {
    ParseResult result = future.get();
    termsByDocument[result.first] = std::move(result.second);
  }

  return termsByDocument;
}

}  // namespace

auto IndexBuilder::build(const Options& options) const -> Result {
  MINISEARCH_LOG_INFO("scanning files...");
  FileScanner scanner;
  std::vector<FileRecord> records = scanner.scan(options.targetPath);
  // Sort records by path to ensure deterministic document IDs across runs
  std::sort(records.begin(), records.end(),
            [](const FileRecord& left, const FileRecord& right) -> bool {
              return recordKey(left.path) < recordKey(right.path);
            });

  IndexStorage storage;
  const std::optional<InvertedIndex> previousIndex =
      loadPreviousIndex(options.indexFile, storage);
  const std::unordered_map<std::string, FileRecord> previousByPath =
      previousIndex.has_value() ? previousRecordsByPath(*previousIndex)
                                : std::unordered_map<std::string, FileRecord>{};

  InvertedIndex index;
  std::unordered_map<InvertedIndex::DocumentId, InvertedIndex::DocumentId>
      reusedIds;
  std::vector<InvertedIndex::DocumentId> idsToParse;
  std::size_t reusedTextFiles = 0;
  
  // determine which records can be reused from the previous index and which text files need to be reparsed
  for (auto& record : records) {
    const std::string key = recordKey(record.path);
    const auto previous = previousByPath.find(key);
    const bool canReuse = previous != previousByPath.end() &&
                          sameIndexedFile(record, previous->second);
    const bool textIndexed = record.textIndexed;
    const InvertedIndex::DocumentId previousId =
        canReuse ? previous->second.id : InvertedIndex::DocumentId{};
    const InvertedIndex::DocumentId id = index.addRecord(std::move(record));

    if (canReuse && textIndexed) {
      reusedIds.emplace(previousId, id);
      ++reusedTextFiles;
    } else if (textIndexed) {
      idsToParse.push_back(id);
    }
  }

  std::vector<std::vector<std::string>> termsByDocument(index.fileCount());
  if (idsToParse.empty()) {
    MINISEARCH_LOG_INFO("no changed text files to parse");
  } else {
    MINISEARCH_LOG_INFO("parsing changed text files...");
    termsByDocument = parseChangedTextFiles(index, idsToParse, options.threads);
  }
  
  // get terms for unchanged files from previous inverted index;
  if (previousIndex.has_value()) {
    for (const auto& [term, postings] : previousIndex->postings()) {
      for (const auto oldId : postings) {
        if(reusedIds.count(oldId)){
          termsByDocument[reusedIds[oldId]].push_back(term);
        }
      }
    }
  }
  
  for (std::size_t id = 0; id < termsByDocument.size(); ++id) {
    index.addTerms(static_cast<InvertedIndex::DocumentId>(id),
                   termsByDocument[id]);
  }

  MINISEARCH_LOG_INFO("incremental index: reused " +
                      std::to_string(reusedTextFiles) +
                      " text file(s), parsed " +
                      std::to_string(idsToParse.size()) + " text file(s)");

  const std::string rootPath =
      IndexRepository::canonicalKey(options.targetPath);
  storage.save(options.indexFile, index, rootPath);
  IndexRepository::saveCurrentIndex(rootPath, options.indexFile);

  MINISEARCH_LOG_INFO("index written to " + options.indexFile.string());
  MINISEARCH_LOG_INFO("current index root set to " + rootPath);

  Result result;
  result.index = std::move(index);
  result.rootPath = rootPath;
  result.indexFile = options.indexFile;
  result.reusedTextFiles = reusedTextFiles;
  result.parsedTextFiles = idsToParse.size();
  return result;
}

}  // namespace minisearch::index
