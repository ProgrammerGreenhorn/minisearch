#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"
#include "minisearch/search/SearchEngine.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::InvertedIndex;
using minisearch::search::SearchEngine;

auto AddDocument(InvertedIndex& index, const std::string& path,
                 const std::vector<std::string>& terms)
    -> InvertedIndex::DocumentId {
  const auto id = index.addRecord(
      FileRecord(std::filesystem::path(path), 123, 456, true, 789));
  index.addTerms(id, terms);
  return id;
}

TEST(SearchEngineTest, FindByNameDelegatesToIndex) {
  InvertedIndex index;
  AddDocument(index, "src/main.cpp", {"alpha"});
  AddDocument(index, "README.md", {"beta"});

  SearchEngine search(index);
  const auto matches = search.findByName("readme", 10);

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].path, std::filesystem::path("README.md"));
}

TEST(SearchEngineTest, GrepTokenizesQueryBeforeSearchingTerms) {
  InvertedIndex index;
  AddDocument(index, "first.txt", {"alpha", "beta"});
  AddDocument(index, "second.txt", {"alpha"});
  AddDocument(index, "third.txt", {"alpha", "beta", "gamma"});

  SearchEngine search(index);
  const auto matches = search.grep("ALPHA, beta!", 10);

  ASSERT_EQ(matches.size(), 2U);
  EXPECT_EQ(matches[0].path, std::filesystem::path("first.txt"));
  EXPECT_EQ(matches[1].path, std::filesystem::path("third.txt"));
}

TEST(SearchEngineTest, FormatRecordIncludesPathAndSize) {
  const FileRecord record(std::filesystem::path("README.md"), 42, 0, true, 1);

  EXPECT_EQ(SearchEngine::formatRecord(record), "README.md (42 bytes)");
}

}  // namespace
