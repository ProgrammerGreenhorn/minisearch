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

auto TempPath(const std::string& filename) -> std::filesystem::path {
  const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (filename + "_" + std::to_string(suffix));
}

TEST(TextParserTest, TokenizeLowercasesWordsAndSplitsOnPunctuation) {
  const auto terms =
      minisearch::index::TextParser::tokenize("Hello, TEST_123 + CMake!");

  const std::vector<std::string> expected{"hello", "test_123", "cmake"};
  EXPECT_EQ(terms, expected);
}

TEST(TextParserTest, TokenizeReturnsEmptyVectorForTextWithoutTerms) {
  const auto terms = minisearch::index::TextParser::tokenize(".,; \t\n---");

  EXPECT_TRUE(terms.empty());
}

TEST(TextParserTest, ParseFileTracksOneBasedLineNumbers) {
  const auto path = TempPath("minisearch_text_parser_lines_test");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output);
    output << "Alpha beta\n";
    output << "\n";
    output << "TEST_123, done!\n";
    output << "alpha";
  }

  minisearch::index::TextParser parser;
  const auto parsedTerms = parser.parseFile(path);

  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  std::vector<std::pair<std::string, std::uint32_t>> actual;
  actual.reserve(parsedTerms.size());
  for (const auto& parsedTerm : parsedTerms) {
    actual.push_back({parsedTerm.term, parsedTerm.line});
  }

  const std::vector<std::pair<std::string, std::uint32_t>> expected{
      {"alpha", 1}, {"beta", 1}, {"test_123", 3}, {"done", 3}, {"alpha", 4}};
  EXPECT_EQ(actual, expected);
}

TEST(TextParserTest, ParseFileReturnsEmptyVectorForMissingFile) {
  minisearch::index::TextParser parser;
  const auto terms = parser.parseFile(TempPath("missing_minisearch_line_file"));

  EXPECT_TRUE(terms.empty());
}

}  // namespace
