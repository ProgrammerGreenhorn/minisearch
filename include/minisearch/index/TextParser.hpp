#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minisearch::index {

struct ParsedTerm {
  std::string term;
  std::uint32_t line = 0;
};

class TextParser {
 public:
  /**
   * @brief Parse a text file into lowercase terms with line numbers.
   *
   * @param file_path File path to read.
   * @return Parsed terms found in the file.
   */
  auto parseFile(const std::filesystem::path& file_path) const
      -> std::vector<ParsedTerm>;

  /**
   * @brief Tokenize text into lowercase alphanumeric terms.
   *
   * @param input_text Text to tokenize.
   * @return Lowercase terms extracted from the input text.
   */
  static auto tokenize(std::string_view input_text) -> std::vector<std::string>;
};

}  // namespace minisearch::index
