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

auto isTermChar(unsigned char ch) -> bool {
  return std::isalnum(ch) || ch == '_';
}

auto lowerToken(std::string_view token) -> std::string {
  std::string lowered;
  lowered.reserve(token.size());
  for (const unsigned char ch : token) {
    lowered.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lowered;
}

}  // namespace

SearchEngine::SearchEngine(const index::InvertedIndex& index) : index_(index) {}

auto SearchEngine::findByName(const std::string& query, std::size_t limit) const
    -> std::vector<index::FileRecord> {
  return index_.findByName(query, limit);
}

auto SearchEngine::grep(const std::string& query,
                        std::size_t limit) const -> std::vector<GrepMatch> {
  return index_.findByTerms(index::TextParser::tokenize(query), limit);
}

auto SearchEngine::grepLines(const std::string& query,
                             std::size_t limit) const -> std::vector<GrepLine> {
  if (limit == 0) {
    return {};
  }

  const std::vector<std::string> terms = index::TextParser::tokenize(query);
  const std::vector<GrepMatch> matches =
      index_.findByTerms(terms, index_.fileCount());

  std::vector<GrepLine> lines;
  for (const auto& match : matches) {
    std::unordered_set<std::uint32_t> wantedLines(match.lines.begin(),
                                                  match.lines.end());
    std::ifstream input(match.record.path);
    std::string text;
    std::uint32_t lineNumber = 1;
    while (std::getline(input, text) && !wantedLines.empty()) {
      if (wantedLines.erase(lineNumber) > 0) {
        lines.push_back(
            {match.record, lineNumber, text, highlightTerms(text, terms)});
        if (lines.size() >= limit) {
          return lines;
        }
      }
      ++lineNumber;
    }
  }

  return lines;
}

auto SearchEngine::formatRecord(const index::FileRecord& record)
    -> std::string {
  std::ostringstream stream;
  stream << record.path.string() << " (" << record.size << " bytes)";
  return stream.str();
}

auto SearchEngine::formatGrepLine(const GrepLine& line) -> std::string {
  std::ostringstream stream;
  stream << line.record.path.string() << ':' << line.line << ":\n  "
         << line.highlightedText;
  return stream.str();
}

auto SearchEngine::highlightTerms(std::string_view text,
                                  const std::vector<std::string>& terms)
    -> std::string {
  if (terms.empty()) {
    return std::string(text);
  }

  const std::unordered_set<std::string> termSet(terms.begin(), terms.end());
  std::string highlighted;
  highlighted.reserve(text.size());

  std::size_t offset = 0;
  while (offset < text.size()) {
    const unsigned char ch = static_cast<unsigned char>(text[offset]);
    if (!isTermChar(ch)) {
      highlighted.push_back(text[offset]);
      ++offset;
      continue;
    }

    const std::size_t tokenStart = offset;
    while (offset < text.size() &&
           isTermChar(static_cast<unsigned char>(text[offset]))) {
      ++offset;
    }

    const std::string_view token = text.substr(tokenStart, offset - tokenStart);
    if (termSet.find(lowerToken(token)) != termSet.end()) {
      highlighted.append(HighlightStart);
      highlighted.append(token);
      highlighted.append(HighlightEnd);
    } else {
      highlighted.append(token);
    }
  }

  return highlighted;
}

}  // namespace minisearch::search
