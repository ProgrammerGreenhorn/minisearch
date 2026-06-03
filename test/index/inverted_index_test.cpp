#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::InvertedIndex;

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

TEST(InvertedIndexTest, AddTermsDeduplicatesTermsWithinDocument) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("src/main.cpp"));

  index.addTerms(id, {"alpha", "beta", "alpha"});

  ASSERT_EQ(index.termCount(), 2U);
  ASSERT_NE(index.postings().find("alpha"), index.postings().end());
  ASSERT_NE(index.postings().find("beta"), index.postings().end());
  EXPECT_EQ(index.postings().at("alpha"),
            std::vector<InvertedIndex::DocumentId>{id});
  EXPECT_EQ(index.postings().at("beta"),
            std::vector<InvertedIndex::DocumentId>{id});
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

TEST(InvertedIndexTest, FindByTermsReturnsDocumentsContainingAllTerms) {
  InvertedIndex index;
  const auto first_id = index.addRecord(MakeRecord("first.txt"));
  const auto second_id = index.addRecord(MakeRecord("second.txt"));
  const auto third_id = index.addRecord(MakeRecord("third.txt"));

  index.addTerms(first_id, {"alpha", "beta"});
  index.addTerms(second_id, {"beta", "gamma"});
  index.addTerms(third_id, {"alpha", "beta", "gamma"});

  const auto matches = index.findByTerms({"alpha", "beta"}, 10);

  ASSERT_EQ(matches.size(), 2U);
  EXPECT_EQ(matches[0].path, std::filesystem::path("first.txt"));
  EXPECT_EQ(matches[1].path, std::filesystem::path("third.txt"));
}

TEST(InvertedIndexTest, FindByTermsReturnsEmptyVectorWhenTermIsMissing) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("first.txt"));
  index.addTerms(id, {"alpha", "beta"});

  EXPECT_TRUE(index.findByTerms({"alpha", "missing"}, 10).empty());
  EXPECT_TRUE(index.findByTerms({}, 10).empty());
}

TEST(InvertedIndexTest, ClearRemovesRecordsAndTerms) {
  InvertedIndex index;
  const auto id = index.addRecord(MakeRecord("first.txt"));
  index.addTerms(id, {"alpha", "beta"});

  index.clear();

  EXPECT_EQ(index.fileCount(), 0U);
  EXPECT_EQ(index.termCount(), 0U);
  EXPECT_TRUE(index.records().empty());
  EXPECT_TRUE(index.postings().empty());
}

}  // namespace
