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

  auto clear() -> void;

  auto addRecord(FileRecord record) -> DocumentId;

  auto addTermOccurrences(DocumentId id,
                          const std::vector<ParsedTerm> &terms) -> void;

  auto replace(std::vector<FileRecord> records,
               std::unordered_map<std::string, LinePostingList> linePostings)
      -> void;

  auto findByName(const std::string &query,
                  std::size_t limit) const -> std::vector<FileRecord>;

  auto findByTerms(const std::vector<std::string> &terms,
                   std::size_t limit) const -> std::vector<LineMatch>;

  auto records() const -> const std::vector<FileRecord> &;

  auto linePostings() const
      -> const std::unordered_map<std::string, LinePostingList> &;

  auto fileCount() const -> std::size_t;

  auto indexedTextFileCount() const -> std::size_t;

  auto termCount() const -> std::size_t;

 private:
  auto recordById(DocumentId id) const -> const FileRecord *;

  static auto lower(std::string value) -> std::string;

  static auto containsCaseInsensitive(const std::string &text,
                                      const std::string &query) -> bool;

  // records are stored in a vector and assigned sequential ids based on their
  // position
  std::vector<FileRecord> records_;

  // line postings map terms to lists of document ids and line numbers where
  // those terms occur
  std::unordered_map<std::string, LinePostingList> linePostings_;
};

}  // namespace minisearch::index
