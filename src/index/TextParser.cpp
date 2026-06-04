#include "minisearch/index/TextParser.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <utility>

namespace minisearch::index {

auto TextParser::parseFile(const std::filesystem::path& file_path) const
    -> std::vector<ParsedTerm> {
  std::ifstream input_stream(file_path);
  if (!input_stream) {
    return {};
  }

  std::vector<ParsedTerm> parsed_terms;
  std::string line_text;
  std::uint32_t line_number = 1;
  while (std::getline(input_stream, line_text)) {
    std::vector<std::string> line_tokens = tokenize(line_text);
    for (auto& token_text : line_tokens) {
      parsed_terms.push_back({std::move(token_text), line_number});
    }
    ++line_number;
  }
  return parsed_terms;
}

auto TextParser::tokenize(std::string_view input_text)
    -> std::vector<std::string> {
  std::vector<std::string> parsed_tokens;
  std::string current_token;

  for (const unsigned char character : input_text) {
    if (std::isalnum(character) || character == '_') {
      current_token.push_back(static_cast<char>(std::tolower(character)));
    } else if (!current_token.empty()) {
      parsed_tokens.push_back(std::move(current_token));
      current_token.clear();
    }
  }

  if (!current_token.empty()) {
    parsed_tokens.push_back(std::move(current_token));
  }

  return parsed_tokens;
}

}  // namespace minisearch::index
