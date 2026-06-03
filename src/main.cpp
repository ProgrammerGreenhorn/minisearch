#include <algorithm>
#include <exception>
#include <filesystem>
#include <future>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minisearch/cli/CommandParser.hpp"
#include "minisearch/index/FileScanner.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/index/TextParser.hpp"
#include "minisearch/shell/InteractiveShell.hpp"
#include "minisearch/util/Logger.hpp"
#include "minisearch/util/ThreadPool.hpp"

namespace {

using minisearch::cli::Command;
using minisearch::cli::CommandOptions;
using minisearch::index::FileRecord;
using minisearch::index::FileScanner;
using minisearch::index::IndexRepository;
using minisearch::index::IndexStorage;
using minisearch::index::InvertedIndex;
using minisearch::index::TextParser;
using minisearch::shell::InteractiveShell;
using minisearch::util::ThreadPool;

auto recordKey(const std::filesystem::path& path) -> std::string {
  return path.lexically_normal().string();
}

auto sameIndexedFile(const FileRecord& current, const FileRecord& previous)
    -> bool {
  return current.size == previous.size &&
         current.textIndexed == previous.textIndexed &&
         current.contentHash == previous.contentHash;
}

auto loadPreviousIndex(const std::filesystem::path& indexFile,
                       const IndexStorage& storage)
    -> std::optional<InvertedIndex> {
  std::error_code error;
  const auto exists = std::filesystem::exists(indexFile, error);
  if (error) {
    MINISEARCH_LOG_WARNING("failed to check existing index file: " +
                           indexFile.string());
    return std::nullopt;
  }

  if (!exists) {
    return std::nullopt;
  }

  try {
    auto index = storage.load(indexFile);
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
  std::vector<std::vector<std::string>> termsByDocument(index.fileCount());
  TextParser parser;
  ThreadPool pool(threads);

  using ParseResult =
      std::pair<InvertedIndex::DocumentId, std::vector<std::string>>;
  std::vector<std::future<ParseResult>> futures;

  for (const auto id : ids) {
    const auto& record = index.records()[id];
    MINISEARCH_LOG_INFO("parsing text file: " + record.path.string());
    futures.push_back(pool.submit([record, &parser]() -> ParseResult {
      return {record.id, parser.parseFile(record.path)};
    }));
  }

  for (auto& future : futures) {
    auto [id, terms] = future.get();
    termsByDocument[id] = std::move(terms);
  }

  return termsByDocument;
}

auto runIndex(const CommandOptions& options) -> int {
  MINISEARCH_LOG_INFO("scanning files...");
  FileScanner scanner;
  auto records = scanner.scan(options.targetPath);
  std::sort(records.begin(), records.end(),
            [](const auto& left, const auto& right) -> bool {
              return recordKey(left.path) < recordKey(right.path);
            });

  IndexStorage storage;
  const auto previousIndex = loadPreviousIndex(options.indexFile, storage);
  const auto previousByPath =
      previousIndex ? previousRecordsByPath(*previousIndex)
                    : std::unordered_map<std::string, FileRecord>{};

  InvertedIndex index;
  std::unordered_map<InvertedIndex::DocumentId, InvertedIndex::DocumentId>
      reusedIds;
  std::vector<InvertedIndex::DocumentId> idsToParse;
  std::size_t reusedTextFiles = 0;

  for (auto& record : records) {
    const auto key = recordKey(record.path);
    const auto previous = previousByPath.find(key);
    const auto canReuse = previous != previousByPath.end() &&
                          sameIndexedFile(record, previous->second);
    const auto textIndexed = record.textIndexed;
    const auto previousId =
        canReuse ? previous->second.id : InvertedIndex::DocumentId{};
    const auto id = index.addRecord(std::move(record));

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
    termsByDocument =
        parseChangedTextFiles(index, idsToParse, options.threads);
  }

  if (previousIndex) {
    for (const auto& [term, postings] : previousIndex->postings()) {
      for (const auto oldId : postings) {
        const auto reused = reusedIds.find(oldId);
        if (reused != reusedIds.end()) {
          termsByDocument[reused->second].push_back(term);
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

  const auto rootPath = IndexRepository::canonicalKey(options.targetPath);
  storage.save(options.indexFile, index, rootPath);
  IndexRepository::saveCurrentIndex(rootPath, options.indexFile);

  MINISEARCH_LOG_INFO("index written to " + options.indexFile.string());
  MINISEARCH_LOG_INFO("current index root set to " + rootPath);
  std::cout << "files: " << index.fileCount() << '\n'
            << "text files: " << index.indexedTextFileCount() << '\n'
            << "terms: " << index.termCount() << '\n';

  InteractiveShell shell(std::move(index), rootPath, options.indexFile);
  return shell.run();
}

auto runOpenCurrent() -> int {
  const auto current = IndexRepository::loadCurrentIndex();
  IndexStorage storage;
  auto index = storage.load(current.indexFile);
  InteractiveShell shell(std::move(index), current.rootPath, current.indexFile);
  return shell.run();
}

}  // namespace

auto main(int argc, char** argv) -> int {
  try {
    minisearch::cli::CommandParser parser;
    const auto options = parser.parse(argc, argv);

    switch (options.command) {
      case Command::OpenCurrent:
        return runOpenCurrent();
      case Command::Help:
        std::cout << minisearch::cli::CommandParser::helpText();
        return 0;
      case Command::Index:
        return runIndex(options);
      case Command::Unknown:
        std::cerr << "unknown command\n"
                  << minisearch::cli::CommandParser::helpText();
        return 2;
    }
  } catch (const std::exception& error) {
    MINISEARCH_LOG_ERROR(error.what());
    return 1;
  }

  return 0;
}
