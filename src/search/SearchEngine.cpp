#include "minisearch/search/SearchEngine.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "minisearch/index/TextParser.hpp"

namespace minisearch::search {

namespace {

constexpr std::string_view HighlightStart = "\x1b[1;33m";
constexpr std::string_view HighlightEnd = "\x1b[0m";

auto isTermChar(unsigned char character) -> bool {
  return std::isalnum(character) || character == '_';
}

auto lowerToken(std::string_view token_text) -> std::string {
  std::string lowered_token;
  lowered_token.reserve(token_text.size());
  for (const unsigned char character : token_text) {
    lowered_token.push_back(static_cast<char>(std::tolower(character)));
  }
  return lowered_token;
}

}  // namespace

SearchEngine::SearchEngine(const index::InvertedIndex& search_index)
    : index_(search_index) {}

auto SearchEngine::findByName(const std::string& query_text, std::size_t limit)
    const -> std::vector<index::FileRecord> {
  return index_.findByName(query_text, limit);
}

auto SearchEngine::grep(const std::string& query_text,
                        std::size_t limit) const -> std::vector<GrepMatch> {
  return index_.findByTerms(index::TextParser::tokenize(query_text), limit);
}

auto SearchEngine::grepLines(const std::string& query_text,
                             std::size_t limit) const -> std::vector<GrepLine> {
  if (limit == 0) {
    return {};
  }

  const std::vector<std::string> query_terms =
      index::TextParser::tokenize(query_text);
  const std::vector<GrepMatch> file_matches =
      index_.findByTerms(query_terms, index_.fileCount());

  std::vector<GrepLine> matching_lines;
  for (const auto& file_match : file_matches) {
    std::unordered_set<std::uint32_t> wanted_lines(file_match.lines.begin(),
                                                   file_match.lines.end());
    std::ifstream input_stream(file_match.record.path);
    std::string line_text;
    std::uint32_t line_number = 1;
    while (std::getline(input_stream, line_text) && !wanted_lines.empty()) {
      if (wanted_lines.erase(line_number) > 0) {
        matching_lines.push_back({file_match.record, line_number, line_text,
                                  highlightTerms(line_text, query_terms)});
        if (matching_lines.size() >= limit) {
          return matching_lines;
        }
      }
      ++line_number;
    }
  }

  return matching_lines;
}

auto SearchEngine::formatRecord(const index::FileRecord& file_record)
    -> std::string {
  std::ostringstream output_stream;
  output_stream << file_record.path.string() << " (" << file_record.size
                << " bytes)";
  return output_stream.str();
}

auto SearchEngine::formatGrepLine(const GrepLine& grep_line) -> std::string {
  std::ostringstream output_stream;
  output_stream << grep_line.record.path.string() << ':' << grep_line.line
                << ":\n  " << grep_line.highlightedText;
  return output_stream.str();
}

auto SearchEngine::highlightTerms(std::string_view source_text,
                                  const std::vector<std::string>& query_terms)
    -> std::string {
  if (query_terms.empty()) {
    return std::string(source_text);
  }

  const std::unordered_set<std::string> query_term_set(query_terms.begin(),
                                                       query_terms.end());
  std::string highlighted_text;
  highlighted_text.reserve(source_text.size());

  std::size_t token_offset = 0;
  while (token_offset < source_text.size()) {
    const unsigned char character =
        static_cast<unsigned char>(source_text[token_offset]);
    if (!isTermChar(character)) {
      highlighted_text.push_back(source_text[token_offset]);
      ++token_offset;
      continue;
    }

    const std::size_t token_start = token_offset;
    while (token_offset < source_text.size() &&
           isTermChar(static_cast<unsigned char>(source_text[token_offset]))) {
      ++token_offset;
    }

    const std::string_view token_text =
        source_text.substr(token_start, token_offset - token_start);
    if (query_term_set.find(lowerToken(token_text)) != query_term_set.end()) {
      highlighted_text.append(HighlightStart);
      highlighted_text.append(token_text);
      highlighted_text.append(HighlightEnd);
    } else {
      highlighted_text.append(token_text);
    }
  }

  return highlighted_text;
}

}  // namespace minisearch::search
