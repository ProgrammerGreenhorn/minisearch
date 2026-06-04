#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "minisearch/index/TextParser.hpp"

namespace {

auto TempPath(const std::string& file_name) -> std::filesystem::path {
  const auto unique_suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (file_name + "_" + std::to_string(unique_suffix));
}

TEST(TextParserTest, TokenizeLowercasesWordsAndSplitsOnPunctuation) {
  const auto parsed_tokens =
      minisearch::index::TextParser::tokenize("Hello, TEST_123 + CMake!");

  const std::vector<std::string> expected_tokens{"hello", "test_123", "cmake"};
  EXPECT_EQ(parsed_tokens, expected_tokens);
}

TEST(TextParserTest, TokenizeReturnsEmptyVectorForTextWithoutTerms) {
  const auto parsed_tokens =
      minisearch::index::TextParser::tokenize(".,; \t\n---");

  EXPECT_TRUE(parsed_tokens.empty());
}

TEST(TextParserTest, ParseFileTracksOneBasedLineNumbers) {
  const auto test_file_path = TempPath("minisearch_text_parser_lines_test");
  {
    std::ofstream output_stream(test_file_path);
    ASSERT_TRUE(output_stream);
    output_stream << "Alpha beta\n";
    output_stream << "\n";
    output_stream << "TEST_123, done!\n";
    output_stream << "alpha";
  }

  minisearch::index::TextParser text_parser;
  const auto parsed_terms = text_parser.parseFile(test_file_path);

  std::error_code ignored_error;
  std::filesystem::remove(test_file_path, ignored_error);

  std::vector<std::pair<std::string, std::uint32_t>> actual_terms;
  actual_terms.reserve(parsed_terms.size());
  for (const auto& parsed_term : parsed_terms) {
    actual_terms.push_back({parsed_term.term, parsed_term.line});
  }

  const std::vector<std::pair<std::string, std::uint32_t>> expected_terms{
      {"alpha", 1}, {"beta", 1}, {"test_123", 3}, {"done", 3}, {"alpha", 4}};
  EXPECT_EQ(actual_terms, expected_terms);
}

TEST(TextParserTest, ParseFileReturnsEmptyVectorForMissingFile) {
  minisearch::index::TextParser text_parser;
  const auto parsed_terms =
      text_parser.parseFile(TempPath("missing_minisearch_line_file"));

  EXPECT_TRUE(parsed_terms.empty());
}

}  // namespace
