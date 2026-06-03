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
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (name + "_" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  auto path() const -> const std::filesystem::path& { return path_; }

 private:
  std::filesystem::path path_;
};

auto AddDocument(InvertedIndex& index, const std::string& path,
                 const std::vector<ParsedTerm>& terms)
    -> InvertedIndex::DocumentId {
  const auto id = index.addRecord(
      FileRecord(std::filesystem::path(path), 123, 456, true, 789));
  index.addTermOccurrences(id, terms);
  return id;
}

auto AddIndexedFile(InvertedIndex& index, const std::filesystem::path& path,
                    const std::string& contents) -> InvertedIndex::DocumentId {
  std::ofstream output(path);
  EXPECT_TRUE(output);
  output << contents;
  output.close();

  const auto id =
      index.addRecord(FileRecord(path, contents.size(), 456, true, 789));
  TextParser parser;
  index.addTermOccurrences(id, parser.parseFile(path));
  return id;
}

TEST(SearchEngineTest, FindByNameDelegatesToIndex) {
  InvertedIndex index;
  AddDocument(index, "src/main.cpp", {ParsedTerm{"alpha", 1}});
  AddDocument(index, "README.md", {ParsedTerm{"beta", 1}});

  SearchEngine search(index);
  const auto matches = search.findByName("readme", 10);

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].path, std::filesystem::path("README.md"));
}

TEST(SearchEngineTest, GrepTokenizesQueryBeforeSearchingTerms) {
  InvertedIndex index;
  AddDocument(index, "first.txt",
              {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1}});
  AddDocument(index, "second.txt", {ParsedTerm{"alpha", 2}});
  AddDocument(
      index, "third.txt",
      {ParsedTerm{"alpha", 4}, ParsedTerm{"beta", 4}, ParsedTerm{"gamma", 5}});

  SearchEngine search(index);
  const auto matches = search.grep("ALPHA, beta!", 10);

  ASSERT_EQ(matches.size(), 2U);
  EXPECT_EQ(matches[0].record.path, std::filesystem::path("first.txt"));
  EXPECT_EQ(matches[0].lines, std::vector<std::uint32_t>({1}));
  EXPECT_EQ(matches[1].record.path, std::filesystem::path("third.txt"));
  EXPECT_EQ(matches[1].lines, std::vector<std::uint32_t>({4}));
}

TEST(SearchEngineTest, GrepLinesReadsMatchingLinesAndHighlightsTerms) {
  ScopedTempDir temp("minisearch_search_engine");
  InvertedIndex index;
  AddIndexedFile(index, temp.path() / "notes.txt",
                 "Alpha beta\nalpha only\nGamma BETA alpha\n");

  SearchEngine search(index);
  const auto lines = search.grepLines("ALPHA, beta!", 10);

  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[0].line, 1U);
  EXPECT_EQ(lines[0].text, "Alpha beta");
  EXPECT_EQ(lines[0].highlightedText,
            "\x1b[1;33mAlpha\x1b[0m \x1b[1;33mbeta\x1b[0m");
  EXPECT_EQ(lines[1].line, 3U);
  EXPECT_EQ(lines[1].text, "Gamma BETA alpha");
  EXPECT_EQ(lines[1].highlightedText,
            "Gamma \x1b[1;33mBETA\x1b[0m \x1b[1;33malpha\x1b[0m");
}

TEST(SearchEngineTest, GrepLinesDefaultDoesNotStopAtFiftyLines) {
  ScopedTempDir temp("minisearch_search_engine_limit");
  InvertedIndex index;

  std::ostringstream contents;
  for (std::size_t line = 0; line < 60; ++line) {
    contents << "Alpha line " << line << '\n';
  }
  AddIndexedFile(index, temp.path() / "many.txt", contents.str());

  SearchEngine search(index);
  const auto lines = search.grepLines("alpha");

  EXPECT_EQ(lines.size(), 60U);
}

TEST(SearchEngineTest, HighlightTermsOnlyHighlightsWholeTokens) {
  const auto highlighted =
      SearchEngine::highlightTerms("Alphabet alpha alpha_beta", {"alpha"});

  EXPECT_EQ(highlighted, "Alphabet \x1b[1;33malpha\x1b[0m alpha_beta");
}

TEST(SearchEngineTest, FormatGrepLineIncludesPathLineAndHighlightedText) {
  const FileRecord record(std::filesystem::path("README.md"), 42, 0, true, 1);
  const SearchEngine::GrepLine line{record, 7, "Alpha",
                                    "\x1b[1;33mAlpha\x1b[0m"};

  EXPECT_EQ(SearchEngine::formatGrepLine(line),
            "README.md:7:\n  \x1b[1;33mAlpha\x1b[0m");
}

TEST(SearchEngineTest, FormatRecordIncludesPathAndSize) {
  const FileRecord record(std::filesystem::path("README.md"), 42, 0, true, 1);

  EXPECT_EQ(SearchEngine::formatRecord(record), "README.md (42 bytes)");
}

}  // namespace
