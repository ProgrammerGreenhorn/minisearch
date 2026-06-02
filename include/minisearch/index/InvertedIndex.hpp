#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "minisearch/index/FileRecord.hpp"

namespace minisearch::index {

class InvertedIndex {
 public:
  using DocumentId = FileRecord::Id;
  using PostingList = std::vector<DocumentId>;

  auto clear() -> void;

  auto addRecord(FileRecord record) -> DocumentId;

  auto addTerms(DocumentId id, const std::vector<std::string> &terms) -> void;

  auto replace(std::vector<FileRecord> records,
               std::unordered_map<std::string, PostingList> postings) -> void;

  auto findByName(const std::string &query,
                  std::size_t limit) const -> std::vector<FileRecord>;

  auto findByTerms(const std::vector<std::string> &terms,
                   std::size_t limit) const -> std::vector<FileRecord>;

  auto records() const -> const std::vector<FileRecord> &;

  auto postings() const -> const std::unordered_map<std::string, PostingList> &;

  auto fileCount() const -> std::size_t;

  auto indexedTextFileCount() const -> std::size_t;

  auto termCount() const -> std::size_t;

 private:
  auto recordById(DocumentId id) const -> const FileRecord *;

  static auto lower(std::string value) -> std::string;

  static auto containsCaseInsensitive(const std::string &text,
                                      const std::string &query) -> bool;

  std::vector<FileRecord> records_;

  std::unordered_map<std::string, PostingList> postings_;
};

}  // namespace minisearch::index
