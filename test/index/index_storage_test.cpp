#include <gtest/gtest.h>

#include <chrono>
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

auto AddRecord(InvertedIndex& index, const std::string& path,
               const std::vector<std::string>& terms)
    -> InvertedIndex::DocumentId {
  const auto id = index.addRecord(
      FileRecord(std::filesystem::path(path), 10, 1234, true, 42));
  index.addTerms(id, terms);
  return id;
}

TEST(IndexStorageTest, SaveCreatesParentDirectoriesAndLoadRoundTripsIndex) {
  ScopedTempDir temp("minisearch_index_storage");
  InvertedIndex index;
  const auto first_id = AddRecord(index, "first.txt", {"alpha", "beta"});
  const auto second_id = AddRecord(index, "second.txt", {"beta", "gamma"});

  const auto index_file = temp.path() / "nested" / "index.pb";
  IndexStorage storage;
  storage.save(index_file, index, temp.path());

  ASSERT_TRUE(std::filesystem::exists(index_file));
  const auto loaded = storage.load(index_file);

  ASSERT_EQ(loaded.records().size(), 2U);
  EXPECT_EQ(loaded.records()[0].id, first_id);
  EXPECT_EQ(loaded.records()[0].path, std::filesystem::path("first.txt"));
  EXPECT_EQ(loaded.records()[1].id, second_id);
  EXPECT_EQ(loaded.records()[1].path, std::filesystem::path("second.txt"));
  EXPECT_EQ(loaded.findByTerms({"beta"}, 10).size(), 2U);
  EXPECT_EQ(loaded.findByTerms({"alpha", "beta"}, 10).size(), 1U);
}

TEST(IndexStorageTest, LoadThrowsForMissingFile) {
  ScopedTempDir temp("minisearch_index_storage_missing");

  IndexStorage storage;
  EXPECT_THROW(storage.load(temp.path() / "missing.pb"), std::runtime_error);
}

TEST(IndexStorageTest, LoadThrowsForCorruptFile) {
  ScopedTempDir temp("minisearch_index_storage_corrupt");
  const auto index_file = temp.path() / "corrupt.pb";
  {
    std::ofstream output(index_file);
    ASSERT_TRUE(output);
    output << "not protobuf";
  }

  IndexStorage storage;
  EXPECT_THROW(storage.load(index_file), std::runtime_error);
}

}  // namespace
