#include "minisearch/index/TextParser.hpp"

#include "minisearch/util/Logger.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace minisearch::index {

namespace {

auto formatTerms(const std::vector<std::string>& terms) -> std::string {
  std::ostringstream stream;
  stream << '[';
  for (std::size_t i = 0; i < terms.size(); ++i) {
    if (i > 0) {
      stream << ", ";
    }
    stream << terms[i];
  }
  stream << ']';
  return stream.str();
}

}  // namespace

auto TextParser::parseFile(const std::filesystem::path& path) const
    -> std::vector<std::string> {
  std::ifstream input(path);
  if (!input) {
    return {};
  }

  std::vector<std::string> terms;
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    auto lineTerms = tokenize(line);
    util::Logger::debug(path.string() + ":" + std::to_string(lineNumber) +
                        " tokens " + formatTerms(lineTerms));
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
