#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"
#include "minisearch/index/TextParser.hpp"
#include "minisearch/search/SearchEngine.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::InvertedIndex;
using minisearch::index::ParsedTerm;
using minisearch::index::TextParser;
using minisearch::search::SearchEngine;

class ScopedTempDir {
 public:
  explicit ScopedTempDir(const std::string& name) {
    const auto unique_suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (name + "_" + std::to_string(unique_suffix));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ignored_error;
    std::filesystem::remove_all(path_, ignored_error);
  }

  auto path() const -> const std::filesystem::path& { return path_; }

 private:
  std::filesystem::path path_;
};

auto AddDocument(InvertedIndex& inverted_index, const std::string& file_path,
                 const std::vector<ParsedTerm>& parsed_terms)
    -> InvertedIndex::DocumentId {
  const auto document_id = inverted_index.addRecord(
      FileRecord(std::filesystem::path(file_path), 123, 456, true, 789));
  inverted_index.addTermOccurrences(document_id, parsed_terms);
  return document_id;
}

auto AddIndexedFile(InvertedIndex& inverted_index,
                    const std::filesystem::path& file_path,
                    const std::string& contents) -> InvertedIndex::DocumentId {
  std::ofstream output_stream(file_path);
  EXPECT_TRUE(output_stream);
  output_stream << contents;
  output_stream.close();

  const auto document_id = inverted_index.addRecord(
      FileRecord(file_path, contents.size(), 456, true, 789));
  TextParser text_parser;
  inverted_index.addTermOccurrences(document_id,
                                    text_parser.parseFile(file_path));
  return document_id;
}

TEST(SearchEngineTest, FindByNameDelegatesToIndex) {
  InvertedIndex inverted_index;
  AddDocument(inverted_index, "src/main.cpp", {ParsedTerm{"alpha", 1}});
  AddDocument(inverted_index, "README.md", {ParsedTerm{"beta", 1}});

  SearchEngine search_engine(inverted_index);
  const auto name_matches = search_engine.findByName("readme", 10);

  ASSERT_EQ(name_matches.size(), 1U);
  EXPECT_EQ(name_matches[0].path, std::filesystem::path("README.md"));
}

TEST(SearchEngineTest, GrepTokenizesQueryBeforeSearchingTerms) {
  InvertedIndex inverted_index;
  AddDocument(inverted_index, "first.txt",
              {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1}});
  AddDocument(inverted_index, "second.txt", {ParsedTerm{"alpha", 2}});
  AddDocument(
      inverted_index, "third.txt",
      {ParsedTerm{"alpha", 4}, ParsedTerm{"beta", 4}, ParsedTerm{"gamma", 5}});

  SearchEngine search_engine(inverted_index);
  const auto grep_matches = search_engine.grep("ALPHA, beta!", 10);

  ASSERT_EQ(grep_matches.size(), 2U);
  EXPECT_EQ(grep_matches[0].record.path, std::filesystem::path("first.txt"));
  EXPECT_EQ(grep_matches[0].lines, std::vector<std::uint32_t>({1}));
  EXPECT_EQ(grep_matches[1].record.path, std::filesystem::path("third.txt"));
  EXPECT_EQ(grep_matches[1].lines, std::vector<std::uint32_t>({4}));
}

TEST(SearchEngineTest, GrepLinesReadsMatchingLinesAndHighlightsTerms) {
  ScopedTempDir temp_dir("minisearch_search_engine");
  InvertedIndex inverted_index;
  AddIndexedFile(inverted_index, temp_dir.path() / "notes.txt",
                 "Alpha beta\nalpha only\nGamma BETA alpha\n");

  SearchEngine search_engine(inverted_index);
  const auto grep_lines = search_engine.grepLines("ALPHA, beta!", 10);

  ASSERT_EQ(grep_lines.size(), 2U);
  EXPECT_EQ(grep_lines[0].line, 1U);
  EXPECT_EQ(grep_lines[0].text, "Alpha beta");
  EXPECT_EQ(grep_lines[0].highlightedText,
            "\x1b[1;33mAlpha\x1b[0m \x1b[1;33mbeta\x1b[0m");
  EXPECT_EQ(grep_lines[1].line, 3U);
  EXPECT_EQ(grep_lines[1].text, "Gamma BETA alpha");
  EXPECT_EQ(grep_lines[1].highlightedText,
            "Gamma \x1b[1;33mBETA\x1b[0m \x1b[1;33malpha\x1b[0m");
}

TEST(SearchEngineTest, GrepLinesDefaultDoesNotStopAtFiftyLines) {
  ScopedTempDir temp_dir("minisearch_search_engine_limit");
  InvertedIndex inverted_index;

  std::ostringstream file_contents;
  for (std::size_t line_number = 0; line_number < 60; ++line_number) {
    file_contents << "Alpha line " << line_number << '\n';
  }
  AddIndexedFile(inverted_index, temp_dir.path() / "many.txt",
                 file_contents.str());

  SearchEngine search_engine(inverted_index);
  const auto grep_lines = search_engine.grepLines("alpha");

  EXPECT_EQ(grep_lines.size(), 60U);
}

TEST(SearchEngineTest, HighlightTermsOnlyHighlightsWholeTokens) {
  const auto highlighted_text =
      SearchEngine::highlightTerms("Alphabet alpha alpha_beta", {"alpha"});

  EXPECT_EQ(highlighted_text, "Alphabet \x1b[1;33malpha\x1b[0m alpha_beta");
}

TEST(SearchEngineTest, FormatGrepLineIncludesPathLineAndHighlightedText) {
  const FileRecord file_record(std::filesystem::path("README.md"), 42, 0, true,
                               1);
  const SearchEngine::GrepLine grep_line{file_record, 7, "Alpha",
                                         "\x1b[1;33mAlpha\x1b[0m"};

  EXPECT_EQ(SearchEngine::formatGrepLine(grep_line),
            "README.md:7:\n  \x1b[1;33mAlpha\x1b[0m");
}

TEST(SearchEngineTest, FormatRecordIncludesPathAndSize) {
  const FileRecord file_record(std::filesystem::path("README.md"), 42, 0, true,
                               1);

  EXPECT_EQ(SearchEngine::formatRecord(file_record), "README.md (42 bytes)");
}

}  // namespace
