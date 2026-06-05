#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "index.pb.h"
#include "minisearch/index/IndexCatalogStorage.hpp"
#include "minisearch/index/IndexSchema.hpp"

namespace {

namespace proto = minisearch::index::proto;

using minisearch::index::IndexCatalogStorage;
using minisearch::index::IndexSchema;
using minisearch::index::ManagedIndex;

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

auto WriteProtoIndexCatalog(const std::filesystem::path& catalog_file,
                            const proto::IndexCatalog& index_catalog) -> void {
  std::ofstream output_stream(catalog_file, std::ios::binary);
  ASSERT_TRUE(output_stream);
  ASSERT_TRUE(index_catalog.SerializeToOstream(&output_stream));
}

auto ReadProtoIndexCatalog(const std::filesystem::path& catalog_file)
    -> proto::IndexCatalog {
  std::ifstream input_stream(catalog_file, std::ios::binary);
  EXPECT_TRUE(input_stream);

  proto::IndexCatalog index_catalog;
  EXPECT_TRUE(index_catalog.ParseFromIstream(&input_stream));
  return index_catalog;
}

TEST(IndexCatalogStorageTest, SaveCreatesParentDirectoriesAndLoadRoundTrips) {
  ScopedTempDir temp_dir("minisearch_index_catalog_storage");
  const auto catalog_file = temp_dir.path() / "nested" / "indexes.pb";
  const std::vector<ManagedIndex> managed_indexes{
      {"first-root", temp_dir.path() / "first.pb", 100},
      {"second-root", temp_dir.path() / "second.pb", 200},
  };

  IndexCatalogStorage index_catalog_storage;
  index_catalog_storage.save(catalog_file, managed_indexes);

  ASSERT_TRUE(std::filesystem::exists(catalog_file));
  const std::vector<ManagedIndex> loaded_indexes =
      index_catalog_storage.load(catalog_file);

  ASSERT_EQ(loaded_indexes.size(), 2U);
  EXPECT_EQ(loaded_indexes[0].rootPath, "first-root");
  EXPECT_EQ(loaded_indexes[0].indexFile, temp_dir.path() / "first.pb");
  EXPECT_EQ(loaded_indexes[0].updatedTime, 100);
  EXPECT_EQ(loaded_indexes[1].rootPath, "second-root");
  EXPECT_EQ(loaded_indexes[1].indexFile, temp_dir.path() / "second.pb");
  EXPECT_EQ(loaded_indexes[1].updatedTime, 200);
}

TEST(IndexCatalogStorageTest, SaveWritesCurrentSchemaVersion) {
  ScopedTempDir temp_dir("minisearch_index_catalog_storage_schema");
  const auto catalog_file = temp_dir.path() / "indexes.pb";

  IndexCatalogStorage index_catalog_storage;
  index_catalog_storage.save(catalog_file,
                             {{"root-path", temp_dir.path() / "index.pb", 1}});

  const proto::IndexCatalog index_catalog = ReadProtoIndexCatalog(catalog_file);
  EXPECT_EQ(index_catalog.version(), IndexSchema::currentIndexCatalogVersion());
  ASSERT_EQ(index_catalog.entries_size(), 1);
  EXPECT_EQ(index_catalog.entries(0).root_path(), "root-path");
}

TEST(IndexCatalogStorageTest, LoadReturnsEmptyForMissingFile) {
  ScopedTempDir temp_dir("minisearch_index_catalog_storage_missing");

  IndexCatalogStorage index_catalog_storage;
  EXPECT_TRUE(
      index_catalog_storage.load(temp_dir.path() / "missing.pb").empty());
}

TEST(IndexCatalogStorageTest, LoadThrowsForFutureSchemaVersion) {
  ScopedTempDir temp_dir("minisearch_index_catalog_storage_future");
  const auto catalog_file = temp_dir.path() / "future.pb";

  proto::IndexCatalog index_catalog;
  index_catalog.set_version(IndexSchema::currentIndexCatalogVersion() + 1);
  WriteProtoIndexCatalog(catalog_file, index_catalog);

  IndexCatalogStorage index_catalog_storage;
  EXPECT_THROW(index_catalog_storage.load(catalog_file), std::runtime_error);
}

TEST(IndexCatalogStorageTest, LoadThrowsForCorruptFile) {
  ScopedTempDir temp_dir("minisearch_index_catalog_storage_corrupt");
  const auto catalog_file = temp_dir.path() / "corrupt.pb";
  {
    std::ofstream output_stream(catalog_file);
    ASSERT_TRUE(output_stream);
    output_stream << "not protobuf";
  }

  IndexCatalogStorage index_catalog_storage;
  EXPECT_THROW(index_catalog_storage.load(catalog_file), std::runtime_error);
}

}  // namespace
