#include "minisearch/index/TextParser.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <utility>

namespace minisearch::index {

auto TextParser::parseFile(const std::filesystem::path& path) const
    -> std::vector<ParsedTerm> {
  std::ifstream input(path);
  if (!input) {
    return {};
  }

  std::vector<ParsedTerm> parsedTerms;
  std::string line;
  std::uint32_t lineNumber = 1;
  while (std::getline(input, line)) {
    std::vector<std::string> lineTerms = tokenize(line);
    for (auto& term : lineTerms) {
      parsedTerms.push_back({std::move(term), lineNumber});
    }
    ++lineNumber;
  }
  return parsedTerms;
}

auto TextParser::tokenize(std::string_view text) -> std::vector<std::string> {
  std::vector<std::string> terms;
  std::string current;

  for (const unsigned char ch : text) {
    if (std::isalnum(ch) || ch == '_') {
      current.push_back(static_cast<char>(std::tolower(ch)));
    } else if (!current.empty()) {
      terms.push_back(std::move(current));
      current.clear();
    }
  }

  if (!current.empty()) {
    terms.push_back(std::move(current));
  }

  return terms;
}

}  // namespace minisearch::index
