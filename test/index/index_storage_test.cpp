#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/index/InvertedIndex.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::IndexStorage;
using minisearch::index::InvertedIndex;
using minisearch::index::ParsedTerm;

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

auto AddRecord(InvertedIndex& search_index, const std::string& file_path,
               const std::vector<ParsedTerm>& parsed_terms)
    -> InvertedIndex::DocumentId {
  const auto document_id = search_index.addRecord(
      FileRecord(std::filesystem::path(file_path), 10, 1234, true, 42));
  search_index.addTermOccurrences(document_id, parsed_terms);
  return document_id;
}

TEST(IndexStorageTest, SaveCreatesParentDirectoriesAndLoadRoundTripsIndex) {
  ScopedTempDir temp_dir("minisearch_index_storage");
  InvertedIndex search_index;
  const auto first_document_id = AddRecord(
      search_index, "first.txt",
      {ParsedTerm{"alpha", 1}, ParsedTerm{"beta", 1}, ParsedTerm{"beta", 2}});
  const auto second_document_id =
      AddRecord(search_index, "second.txt",
                {ParsedTerm{"beta", 3}, ParsedTerm{"gamma", 3}});

  const auto index_file = temp_dir.path() / "nested" / "index.pb";
  IndexStorage index_storage;
  index_storage.save(index_file, search_index, temp_dir.path());

  ASSERT_TRUE(std::filesystem::exists(index_file));
  const auto loaded_index = index_storage.load(index_file);

  ASSERT_EQ(loaded_index.records().size(), 2U);
  EXPECT_EQ(loaded_index.records()[0].id, first_document_id);
  EXPECT_EQ(loaded_index.records()[0].path, std::filesystem::path("first.txt"));
  EXPECT_EQ(loaded_index.records()[1].id, second_document_id);
  EXPECT_EQ(loaded_index.records()[1].path,
            std::filesystem::path("second.txt"));

  const auto beta_matches = loaded_index.findByTerms({"beta"}, 10);
  ASSERT_EQ(beta_matches.size(), 2U);
  EXPECT_EQ(beta_matches[0].record.path, std::filesystem::path("first.txt"));
  EXPECT_EQ(beta_matches[0].lines, std::vector<std::uint32_t>({1, 2}));
  EXPECT_EQ(beta_matches[1].record.path, std::filesystem::path("second.txt"));
  EXPECT_EQ(beta_matches[1].lines, std::vector<std::uint32_t>({3}));

  const auto same_line_matches =
      loaded_index.findByTerms({"alpha", "beta"}, 10);
  ASSERT_EQ(same_line_matches.size(), 1U);
  EXPECT_EQ(same_line_matches[0].record.path,
            std::filesystem::path("first.txt"));
  EXPECT_EQ(same_line_matches[0].lines, std::vector<std::uint32_t>({1}));
}

TEST(IndexStorageTest, LoadThrowsForMissingFile) {
  ScopedTempDir temp_dir("minisearch_index_storage_missing");

  IndexStorage index_storage;
  EXPECT_THROW(index_storage.load(temp_dir.path() / "missing.pb"),
               std::runtime_error);
}

TEST(IndexStorageTest, LoadThrowsForCorruptFile) {
  ScopedTempDir temp_dir("minisearch_index_storage_corrupt");
  const auto index_file = temp_dir.path() / "corrupt.pb";
  {
    std::ofstream output_stream(index_file);
    ASSERT_TRUE(output_stream);
    output_stream << "not protobuf";
  }

  IndexStorage index_storage;
  EXPECT_THROW(index_storage.load(index_file), std::runtime_error);
}

}  // namespace
