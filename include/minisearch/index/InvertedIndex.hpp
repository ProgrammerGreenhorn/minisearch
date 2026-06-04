#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/TextParser.hpp"

namespace minisearch::index {

class InvertedIndex {
 public:
  using DocumentId = FileRecord::Id;

  struct LinePosting {
    DocumentId documentId = 0;
    std::vector<std::uint32_t> lines;
  };

  using LinePostingList = std::vector<LinePosting>;

  struct LineMatch {
    FileRecord record;
    std::vector<std::uint32_t> lines;
  };

  /**
   * @brief Remove all records and postings from the index.
   */
  auto clear() -> void;

  /**
   * @brief Add a file record and assign its document id.
   *
   * @param file_record File record to add.
   * @return Assigned document id.
   */
  auto addRecord(FileRecord file_record) -> DocumentId;

  /**
   * @brief Add parsed term occurrences for a document.
   *
   * @param document_id Document id receiving the term occurrences.
   * @param parsed_terms Parsed terms with line numbers.
   */
  auto addTermOccurrences(DocumentId document_id,
                          const std::vector<ParsedTerm> &parsed_terms) -> void;

  /**
   * @brief Replace the complete contents of the index.
   *
   * @param file_records File records to store.
   * @param line_postings Term-to-document line postings to store.
   */
  auto replace(std::vector<FileRecord> file_records,
               std::unordered_map<std::string, LinePostingList> line_postings)
      -> void;

  /**
   * @brief Search indexed file names and paths.
   *
   * @param query_text Case-insensitive substring query.
   * @param limit Maximum number of records to return.
   * @return Matching file records.
   */
  auto findByName(const std::string &query_text,
                  std::size_t limit) const -> std::vector<FileRecord>;

  /**
   * @brief Search indexed text terms on shared lines.
   *
   * @param query_terms Lowercase query terms to match.
   * @param limit Maximum number of matching files to return.
   * @return Matching files with line numbers where all terms occur.
   */
  auto findByTerms(const std::vector<std::string> &query_terms,
                   std::size_t limit) const -> std::vector<LineMatch>;

  /**
   * @brief Get all file records in document-id order.
   *
   * @return File records stored in the index.
   */
  auto records() const -> const std::vector<FileRecord> &;

  /**
   * @brief Get all term line postings.
   *
   * @return Mapping from term to document line postings.
   */
  auto linePostings() const
      -> const std::unordered_map<std::string, LinePostingList> &;

  /**
   * @brief Count all indexed file records.
   *
   * @return Number of file records.
   */
  auto fileCount() const -> std::size_t;

  /**
   * @brief Count records whose text content was indexed.
   *
   * @return Number of text-indexed file records.
   */
  auto indexedTextFileCount() const -> std::size_t;

  /**
   * @brief Count unique indexed terms.
   *
   * @return Number of terms in the posting map.
   */
  auto termCount() const -> std::size_t;

 private:
  /**
   * @brief Look up a file record by document id.
   *
   * @param document_id Document id to resolve.
   * @return Matching record, or nullptr if the id is invalid.
   */
  auto recordById(DocumentId document_id) const -> const FileRecord *;

  /**
   * @brief Convert text to lowercase.
   *
   * @param input_text Text to convert.
   * @return Lowercase copy of the input text.
   */
  static auto lower(std::string input_text) -> std::string;

  /**
   * @brief Check whether text contains query without case sensitivity.
   *
   * @param text Text to search.
   * @param query_text Query substring.
   * @return True if text contains query case-insensitively.
   */
  static auto containsCaseInsensitive(const std::string &text,
                                      const std::string &query_text) -> bool;

  // records are stored in a vector and assigned sequential ids based on their
  // position
  std::vector<FileRecord> records_;

  // line postings map terms to lists of document ids and line numbers where
  // those terms occur
  std::unordered_map<std::string, LinePostingList> linePostings_;
};

}  // namespace minisearch::index
