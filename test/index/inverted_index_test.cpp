#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::InvertedIndex;
using minisearch::index::ParsedTerm;

auto MakeRecord(const std::string& path,
                bool text_indexed = true) -> FileRecord {
  return FileRecord(std::filesystem::path(path), 100, 12345, text_indexed, 999);
}

TEST(InvertedIndexTest, AddRecordAssignsSequentialIdsAndStoresRecords) {
  InvertedIndex index;

  const auto first_id = index.addRecord(MakeRecord("src/main.cpp"));
  const auto second_id = index.addRecord(MakeRecord("README.md", false));

  EXPECT_EQ(first_id, 0U);
  EXPECT_EQ(second_id, 1U);
  ASSERT_EQ(index.records().size(), 2U);
  EXPECT_EQ(index.records()[0].id, 0U);
  EXPECT_EQ(index.records()[1].id, 1U);
  EXPECT_EQ(index.fileCount(), 2U);
  EXPECT_EQ(index.indexedTextFileCount(), 1U);
}

TEST(InvertedIndexTest, AddTermOccurrencesStoresUniqueLinesPerTerm) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("src/main.cpp"));

  index.addTermOccurrences(id,
                           {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1},
                            ParsedTerm{"alpha", 3}, ParsedTerm{"alpha", 3}});

  ASSERT_EQ(index.termCount(), 2U);
  ASSERT_NE(index.linePostings().find("alpha"), index.linePostings().end());
  ASSERT_EQ(index.linePostings().at("alpha").size(), 1U);
  EXPECT_EQ(index.linePostings().at("alpha")[0].documentId, id);
  EXPECT_EQ(index.linePostings().at("alpha")[0].lines,
            std::vector<std::uint32_t>({1, 3}));
}

TEST(InvertedIndexTest, FindByNameMatchesCaseInsensitivePathAndHonorsLimit) {
  InvertedIndex index;
  index.addRecord(MakeRecord("src/Main.cpp"));
  index.addRecord(MakeRecord("include/minisearch/main.hpp"));
  index.addRecord(MakeRecord("README.md"));

  const auto matches = index.findByName("MAIN", 1);

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].path, std::filesystem::path("src/Main.cpp"));
}

TEST(InvertedIndexTest, FindByTermsReturnsLinesContainingAllTerms) {
  InvertedIndex index;
  const auto first_id = index.addRecord(MakeRecord("first.txt"));
  const auto second_id = index.addRecord(MakeRecord("second.txt"));
  const auto third_id = index.addRecord(MakeRecord("third.txt"));

  index.addTermOccurrences(first_id,
                           {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1},
                            ParsedTerm{"alpha", 2}, ParsedTerm{"gamma", 2}});
  index.addTermOccurrences(second_id,
                           {ParsedTerm{"alpha", 4}, ParsedTerm{"beta", 5}});
  index.addTermOccurrences(third_id, {ParsedTerm{"beta", 1}});

  const auto matches = index.findByTerms({"alpha", "beta"}, 10);

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].record.path, std::filesystem::path("first.txt"));
  EXPECT_EQ(matches[0].lines, std::vector<std::uint32_t>({1}));
}

TEST(InvertedIndexTest, FindByTermsHonorsLimit) {
  InvertedIndex index;
  const auto first_id = index.addRecord(MakeRecord("first.txt"));
  const auto second_id = index.addRecord(MakeRecord("second.txt"));

  index.addTermOccurrences(first_id, {ParsedTerm{"alpha", 1}});
  index.addTermOccurrences(second_id, {ParsedTerm{"alpha", 2}});

  const auto matches = index.findByTerms({"alpha"}, 1);

  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].record.path, std::filesystem::path("first.txt"));
}

TEST(InvertedIndexTest, FindByTermsReturnsEmptyVectorWhenMissing) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("first.txt"));
  index.addTermOccurrences(id, {ParsedTerm{"alpha", 1}});

  EXPECT_TRUE(index.findByTerms({"alpha", "missing"}, 10).empty());
  EXPECT_TRUE(index.findByTerms({}, 10).empty());
}

TEST(InvertedIndexTest, ClearRemovesRecordsAndTerms) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("first.txt"));
  index.addTermOccurrences(id, {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 2}});

  index.clear();

  EXPECT_EQ(index.fileCount(), 0U);
  EXPECT_EQ(index.termCount(), 0U);
  EXPECT_TRUE(index.records().empty());
  EXPECT_TRUE(index.linePostings().empty());
}

}  // namespace
