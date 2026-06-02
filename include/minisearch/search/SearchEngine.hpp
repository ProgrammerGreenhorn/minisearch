#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::search {

class SearchEngine {
 public:
  explicit SearchEngine(const index::InvertedIndex& index);

  auto findByName(const std::string& query, std::size_t limit = 50) const
      -> std::vector<index::FileRecord>;
  auto grep(const std::string& query,
            std::size_t limit = 50) const -> std::vector<index::FileRecord>;

  static auto formatRecord(const index::FileRecord& record) -> std::string;

 private:
  const index::InvertedIndex& index_;
};

}  // namespace minisearch::search
