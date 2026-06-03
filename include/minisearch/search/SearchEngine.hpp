#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::search {

inline constexpr std::size_t Limit = std::numeric_limits<std::size_t>::max();

class SearchEngine {
 public:
  using GrepMatch = index::InvertedIndex::LineMatch;

  struct GrepLine {
    index::FileRecord record;
    std::uint32_t line = 0;
    std::string text;
    std::string highlightedText;
  };

  explicit SearchEngine(const index::InvertedIndex& index);

  auto findByName(const std::string& query, std::size_t limit = Limit) const
      -> std::vector<index::FileRecord>;
  auto grep(const std::string& query,
            std::size_t limit = Limit) const -> std::vector<GrepMatch>;
  auto grepLines(const std::string& query,
                 std::size_t limit = Limit) const -> std::vector<GrepLine>;

  static auto formatRecord(const index::FileRecord& record) -> std::string;
  static auto formatGrepLine(const GrepLine& line) -> std::string;
  static auto highlightTerms(std::string_view text,
                             const std::vector<std::string>& terms)
      -> std::string;

 private:
  const index::InvertedIndex& index_;
};

}  // namespace minisearch::search
