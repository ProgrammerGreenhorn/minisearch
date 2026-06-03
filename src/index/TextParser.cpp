#include "minisearch/index/TextParser.hpp"

#include <cctype>
#include <fstream>
#include <iterator>

namespace minisearch::index {

auto TextParser::parseFile(const std::filesystem::path& path) const
    -> std::vector<std::string> {
  std::ifstream input(path);
  if (!input) {
    return {};
  }

  std::vector<std::string> terms;
  std::string line;
  while (std::getline(input, line)) {
    std::vector<std::string> lineTerms = tokenize(line);
    terms.insert(terms.end(), std::make_move_iterator(lineTerms.begin()),
                 std::make_move_iterator(lineTerms.end()));
  }
  return terms;
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
