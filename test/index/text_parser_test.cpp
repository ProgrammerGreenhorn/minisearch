#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
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

TEST(TextParserTest, ParseFileTokenizesAllLinesInOrder) {
  const auto path = TempPath("minisearch_text_parser_test");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output);
    output << "Alpha beta\n";
    output << "TEST_123, done!";
  }

  minisearch::index::TextParser parser;
  const auto terms = parser.parseFile(path);

  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  const std::vector<std::string> expected{"alpha", "beta", "test_123", "done"};
  EXPECT_EQ(terms, expected);
}

TEST(TextParserTest, ParseFileReturnsEmptyVectorForMissingFile) {
  minisearch::index::TextParser parser;
  const auto terms = parser.parseFile(TempPath("missing_minisearch_file"));

  EXPECT_TRUE(terms.empty());
}

}  // namespace
