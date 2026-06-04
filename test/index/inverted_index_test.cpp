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

auto MakeRecord(const std::string& file_path,
                bool text_indexed = true) -> FileRecord {
  return FileRecord(std::filesystem::path(file_path), 100, 12345, text_indexed,
                    999);
}

TEST(InvertedIndexTest, AddRecordAssignsSequentialIdsAndStoresRecords) {
  InvertedIndex inverted_index;

  const auto first_document_id =
      inverted_index.addRecord(MakeRecord("src/main.cpp"));
  const auto second_document_id =
      inverted_index.addRecord(MakeRecord("README.md", false));

  EXPECT_EQ(first_document_id, 0U);
  EXPECT_EQ(second_document_id, 1U);
  ASSERT_EQ(inverted_index.records().size(), 2U);
  EXPECT_EQ(inverted_index.records()[0].id, 0U);
  EXPECT_EQ(inverted_index.records()[1].id, 1U);
  EXPECT_EQ(inverted_index.fileCount(), 2U);
  EXPECT_EQ(inverted_index.indexedTextFileCount(), 1U);
}

TEST(InvertedIndexTest, AddTermOccurrencesStoresUniqueLinesPerTerm) {
  InvertedIndex inverted_index;
  const auto document_id = inverted_index.addRecord(MakeRecord("src/main.cpp"));

  inverted_index.addTermOccurrences(
      document_id, {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1},
                    ParsedTerm{"alpha", 3}, ParsedTerm{"alpha", 3}});

  ASSERT_EQ(inverted_index.termCount(), 2U);
  ASSERT_NE(inverted_index.linePostings().find("alpha"),
            inverted_index.linePostings().end());
  ASSERT_EQ(inverted_index.linePostings().at("alpha").size(), 1U);
  EXPECT_EQ(inverted_index.linePostings().at("alpha")[0].documentId,
            document_id);
  EXPECT_EQ(inverted_index.linePostings().at("alpha")[0].lines,
            std::vector<std::uint32_t>({1, 3}));
}

TEST(InvertedIndexTest, FindByNameMatchesCaseInsensitivePathAndHonorsLimit) {
  InvertedIndex inverted_index;
  inverted_index.addRecord(MakeRecord("src/Main.cpp"));
  inverted_index.addRecord(MakeRecord("include/minisearch/main.hpp"));
  inverted_index.addRecord(MakeRecord("README.md"));

  const auto name_matches = inverted_index.findByName("MAIN", 1);

  ASSERT_EQ(name_matches.size(), 1U);
  EXPECT_EQ(name_matches[0].path, std::filesystem::path("src/Main.cpp"));
}

TEST(InvertedIndexTest, FindByTermsReturnsLinesContainingAllTerms) {
  InvertedIndex inverted_index;
  const auto first_document_id =
      inverted_index.addRecord(MakeRecord("first.txt"));
  const auto second_document_id =
      inverted_index.addRecord(MakeRecord("second.txt"));
  const auto third_document_id =
      inverted_index.addRecord(MakeRecord("third.txt"));

  inverted_index.addTermOccurrences(
      first_document_id, {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1},
                          ParsedTerm{"alpha", 2}, ParsedTerm{"gamma", 2}});
  inverted_index.addTermOccurrences(
      second_document_id, {ParsedTerm{"alpha", 4}, ParsedTerm{"beta", 5}});
  inverted_index.addTermOccurrences(third_document_id, {ParsedTerm{"beta", 1}});

  const auto term_matches = inverted_index.findByTerms({"alpha", "beta"}, 10);

  ASSERT_EQ(term_matches.size(), 1U);
  EXPECT_EQ(term_matches[0].record.path, std::filesystem::path("first.txt"));
  EXPECT_EQ(term_matches[0].lines, std::vector<std::uint32_t>({1}));
}

TEST(InvertedIndexTest, FindByTermsHonorsLimit) {
  InvertedIndex inverted_index;
  const auto first_document_id =
      inverted_index.addRecord(MakeRecord("first.txt"));
  const auto second_document_id =
      inverted_index.addRecord(MakeRecord("second.txt"));

  inverted_index.addTermOccurrences(first_document_id,
                                    {ParsedTerm{"alpha", 1}});
  inverted_index.addTermOccurrences(second_document_id,
                                    {ParsedTerm{"alpha", 2}});

  const auto term_matches = inverted_index.findByTerms({"alpha"}, 1);

  ASSERT_EQ(term_matches.size(), 1U);
  EXPECT_EQ(term_matches[0].record.path, std::filesystem::path("first.txt"));
}

TEST(InvertedIndexTest, FindByTermsReturnsEmptyVectorWhenMissing) {
  InvertedIndex inverted_index;
  const auto document_id = inverted_index.addRecord(MakeRecord("first.txt"));
  inverted_index.addTermOccurrences(document_id, {ParsedTerm{"alpha", 1}});

  EXPECT_TRUE(inverted_index.findByTerms({"alpha", "missing"}, 10).empty());
  EXPECT_TRUE(inverted_index.findByTerms({}, 10).empty());
}

TEST(InvertedIndexTest, ClearRemovesRecordsAndTerms) {
  InvertedIndex inverted_index;
  const auto document_id = inverted_index.addRecord(MakeRecord("first.txt"));
  inverted_index.addTermOccurrences(
      document_id, {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 2}});

  inverted_index.clear();

  EXPECT_EQ(inverted_index.fileCount(), 0U);
  EXPECT_EQ(inverted_index.termCount(), 0U);
  EXPECT_TRUE(inverted_index.records().empty());
  EXPECT_TRUE(inverted_index.linePostings().empty());
}

}  // namespace
