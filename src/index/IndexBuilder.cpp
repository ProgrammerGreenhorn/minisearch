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

auto recordKey(const std::filesystem::path& record_path) -> std::string {
  return record_path.lexically_normal().string();
}

auto sameIndexedFile(const FileRecord& current_record,
                     const FileRecord& previous_record) -> bool {
  return current_record.size == previous_record.size &&
         current_record.textIndexed == previous_record.textIndexed &&
         current_record.contentHash == previous_record.contentHash;
}

auto loadPreviousIndex(const std::filesystem::path& index_file,
                       const IndexStorage& index_storage)
    -> std::optional<InvertedIndex> {
  std::error_code exists_error;
  const bool index_exists = std::filesystem::exists(index_file, exists_error);
  if (exists_error) {
    MINISEARCH_LOG_WARNING("failed to check existing index file: " +
                           index_file.string());
    return std::nullopt;
  }

  if (!index_exists) {
    return std::nullopt;
  }

  try {
    InvertedIndex previous_index = index_storage.load(index_file);
    MINISEARCH_LOG_INFO("loaded existing index: " + index_file.string());
    return previous_index;
  } catch (const std::exception& exception) {
    MINISEARCH_LOG_WARNING("failed to load existing index; rebuilding: " +
                           std::string(exception.what()));
    return std::nullopt;
  }
}

auto previousRecordsByPath(const InvertedIndex& previous_index)
    -> std::unordered_map<std::string, FileRecord> {
  std::unordered_map<std::string, FileRecord> records_by_path;
  records_by_path.reserve(previous_index.records().size());
  for (const auto& file_record : previous_index.records()) {
    records_by_path.emplace(recordKey(file_record.path), file_record);
  }
  return records_by_path;
}

auto parseChangedTextFiles(
    const InvertedIndex& current_index,
    const std::vector<InvertedIndex::DocumentId>& document_ids_to_parse,
    std::size_t thread_count) -> std::vector<std::vector<ParsedTerm>> {
  struct ParseResult {
    InvertedIndex::DocumentId document_id;
    std::vector<ParsedTerm> parsed_terms;
  };

  std::vector<std::vector<ParsedTerm>> terms_by_document(
      current_index.fileCount());
  TextParser text_parser;
  ThreadPool thread_pool(thread_count);

  std::vector<std::future<ParseResult>> parse_futures;

  for (const auto document_id : document_ids_to_parse) {
    const FileRecord& file_record = current_index.records()[document_id];
    MINISEARCH_LOG_INFO("parsing text file: " + file_record.path.string());
    parse_futures.push_back(
        thread_pool.submit([file_record, &text_parser]() -> ParseResult {
          return {file_record.id, text_parser.parseFile(file_record.path)};
        }));
  }

  for (auto& parse_future : parse_futures) {
    ParseResult parse_result = parse_future.get();
    terms_by_document[parse_result.document_id] =
        std::move(parse_result.parsed_terms);
  }

  return terms_by_document;
}

}  // namespace

auto IndexBuilder::build(const Options& build_options) const -> Result {
  MINISEARCH_LOG_INFO("scanning files...");
  FileScanner file_scanner(build_options.scannerOptions);
  std::vector<FileRecord> scanned_records =
      file_scanner.scan(build_options.targetPath);
  // Sort records by path to ensure deterministic document IDs across runs
  std::sort(scanned_records.begin(), scanned_records.end(),
            [](const FileRecord& left_record,
               const FileRecord& right_record) -> bool {
              return recordKey(left_record.path) < recordKey(right_record.path);
            });

  IndexStorage index_storage;
  const std::optional<InvertedIndex> previous_index =
      loadPreviousIndex(build_options.indexFile, index_storage);
  const std::unordered_map<std::string, FileRecord> previous_records_by_path =
      previous_index.has_value()
          ? previousRecordsByPath(*previous_index)
          : std::unordered_map<std::string, FileRecord>{};

  InvertedIndex rebuilt_index;
  std::unordered_map<InvertedIndex::DocumentId, InvertedIndex::DocumentId>
      reused_document_ids;
  std::vector<InvertedIndex::DocumentId> document_ids_to_parse;
  std::size_t reused_text_files = 0;

  for (auto& file_record : scanned_records) {
    const std::string record_path_key = recordKey(file_record.path);
    const auto previous_record_entry =
        previous_records_by_path.find(record_path_key);
    const bool can_reuse_record =
        previous_record_entry != previous_records_by_path.end() &&
        sameIndexedFile(file_record, previous_record_entry->second);
    const bool text_indexed = file_record.textIndexed;
    const InvertedIndex::DocumentId previous_document_id =
        can_reuse_record ? previous_record_entry->second.id
                         : InvertedIndex::DocumentId{};
    const InvertedIndex::DocumentId document_id =
        rebuilt_index.addRecord(std::move(file_record));

    if (can_reuse_record && text_indexed) {
      reused_document_ids.emplace(previous_document_id, document_id);
      ++reused_text_files;
    } else if (text_indexed) {
      document_ids_to_parse.push_back(document_id);
    }
  }
  // The terms_by_document structure is:
  // [
  //   [ { term: "term1", line: 1 }, { term: "term2", line: 1 }, ... ], // terms
  //   for document 0 [ { term: "term3", line: 2 }, { term: "term4", line: 2 },
  //   ... ], // terms for document 1
  //   ...
  // ]
  std::vector<std::vector<ParsedTerm>> terms_by_document(
      rebuilt_index.fileCount());
  if (document_ids_to_parse.empty()) {
    MINISEARCH_LOG_INFO("no changed text files to parse");
  } else {
    MINISEARCH_LOG_INFO("parsing changed text files...");
    terms_by_document = parseChangedTextFiles(
        rebuilt_index, document_ids_to_parse, build_options.threads);
  }

  if (previous_index.has_value()) {
    // Each entry in linePostings maps one term to the list of documents and
    // line numbers where that term appeared in the previous index. For reused
    // files, copy those term occurrences into the new document id so unchanged
    // files do not need to be parsed again.
    for (const auto& [term_text, previous_postings] :
         previous_index->linePostings()) {
      for (const auto& line_posting : previous_postings) {
        const auto reused_document_id_entry =
            reused_document_ids.find(line_posting.documentId);
        if (reused_document_id_entry == reused_document_ids.end()) {
          continue;
        }
        const auto new_document_id = reused_document_id_entry->second;
        // The term and line number together form a ParsedTerm for the new id.
        for (const auto line_number : line_posting.lines) {
          terms_by_document[new_document_id].push_back(
              {term_text, line_number});
        }
      }
    }
  }

  for (std::size_t document_index = 0;
       document_index < terms_by_document.size(); ++document_index) {
    rebuilt_index.addTermOccurrences(
        static_cast<InvertedIndex::DocumentId>(document_index),
        terms_by_document[document_index]);
  }

  MINISEARCH_LOG_INFO(
      "incremental index: reused " + std::to_string(reused_text_files) +
      " text file(s), parsed " + std::to_string(document_ids_to_parse.size()) +
      " text file(s)");

  const std::string root_path =
      IndexRepository::canonicalKey(build_options.targetPath);
  index_storage.save(build_options.indexFile, rebuilt_index, root_path);
  IndexRepository::saveCurrentIndex(root_path, build_options.indexFile);

  MINISEARCH_LOG_INFO("index written to " + build_options.indexFile.string());
  MINISEARCH_LOG_INFO("current index root set to " + root_path);

  Result build_result;
  build_result.index = std::move(rebuilt_index);
  build_result.rootPath = root_path;
  build_result.indexFile = build_options.indexFile;
  build_result.reusedTextFiles = reused_text_files;
  build_result.parsedTextFiles = document_ids_to_parse.size();
  return build_result;
}

}  // namespace minisearch::index
