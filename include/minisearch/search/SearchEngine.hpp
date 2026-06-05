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

/**
 * @brief Query an in-memory inverted index for names and content.
 *
 * SearchEngine wraps the lower-level inverted index with display-oriented
 * helpers for filename search, grep-style search, and result formatting.
 */
class SearchEngine {
 public:
  using GrepMatch = index::InvertedIndex::LineMatch;

  struct GrepLine {
    index::FileRecord record;
    std::uint32_t line = 0;
    std::string text;
    std::string highlightedText;
  };

  /**
   * @brief Create a search engine over an existing index.
   *
   * @param search_index Index to query; the caller must keep it alive.
   */
  explicit SearchEngine(const index::InvertedIndex& search_index);

  /**
   * @brief Search indexed file names and paths.
   *
   * @param query_text Case-insensitive substring query.
   * @param limit Maximum number of file records to return.
   * @return Matching file records.
   */
  auto findByName(const std::string& query_text, std::size_t limit = Limit)
      const -> std::vector<index::FileRecord>;

  /**
   * @brief Search indexed text content by tokenized query terms.
   *
   * @param query_text Text query to tokenize and match.
   * @param limit Maximum number of matching files to return.
   * @return Matching files with line numbers for shared query-term lines.
   */
  auto grep(const std::string& query_text,
            std::size_t limit = Limit) const -> std::vector<GrepMatch>;

  /**
   * @brief Search indexed text content and load matching source lines.
   *
   * @param query_text Text query to tokenize and match.
   * @param limit Maximum number of matching lines to return.
   * @return Matching lines with highlighted display text.
   */
  auto grepLines(const std::string& query_text,
                 std::size_t limit = Limit) const -> std::vector<GrepLine>;

  /**
   * @brief Format a file record for display.
   *
   * @param file_record File record to format.
   * @return Human-readable file path and size.
   */
  static auto formatRecord(const index::FileRecord& file_record) -> std::string;

  /**
   * @brief Format a grep line for display.
   *
   * @param grep_line Grep line to format.
   * @return Human-readable file path, line number, and highlighted text.
   */
  static auto formatGrepLine(const GrepLine& grep_line) -> std::string;

  /**
   * @brief Highlight query terms in a line of text.
   *
   * @param source_text Source text to highlight.
   * @param query_terms Lowercase query terms to highlight.
   * @return Text with terminal highlight escape sequences.
   */
  static auto highlightTerms(std::string_view source_text,
                             const std::vector<std::string>& query_terms)
      -> std::string;

 private:
  const index::InvertedIndex& index_;
};

}  // namespace minisearch::search
