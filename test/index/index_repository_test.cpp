#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "index.pb.h"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexSchema.hpp"

namespace {

namespace proto = minisearch::index::proto;

using minisearch::index::IndexRepository;
using minisearch::index::IndexSchema;

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

class ScopedHome {
 public:
  explicit ScopedHome(const std::filesystem::path& home_path) {
    const char* previous_home_value = std::getenv("HOME");
    if (previous_home_value != nullptr) {
      old_home_ = previous_home_value;
      had_old_home_ = true;
    }
    setenv("HOME", home_path.string().c_str(), 1);
  }

  ~ScopedHome() {
    if (had_old_home_) {
      setenv("HOME", old_home_.c_str(), 1);
    } else {
      unsetenv("HOME");
    }
  }

 private:
  std::string old_home_;
  bool had_old_home_ = false;
};

auto WriteProtoCurrentIndex(const proto::CurrentIndex& current_index) -> void {
  std::filesystem::create_directories(
      IndexRepository::currentPointerFile().parent_path());
  std::ofstream output_stream(IndexRepository::currentPointerFile(),
                              std::ios::binary);
  ASSERT_TRUE(output_stream);
  ASSERT_TRUE(current_index.SerializeToOstream(&output_stream));
}

auto ReadProtoCurrentIndex() -> proto::CurrentIndex {
  std::ifstream input_stream(IndexRepository::currentPointerFile(),
                             std::ios::binary);
  EXPECT_TRUE(input_stream);

  proto::CurrentIndex current_index;
  EXPECT_TRUE(current_index.ParseFromIstream(&input_stream));
  return current_index;
}

auto WriteProtoIndexCatalog(const proto::IndexCatalog& index_catalog) -> void {
  std::filesystem::create_directories(
      IndexRepository::indexCatalogFile().parent_path());
  std::ofstream output_stream(IndexRepository::indexCatalogFile(),
                              std::ios::binary);
  ASSERT_TRUE(output_stream);
  ASSERT_TRUE(index_catalog.SerializeToOstream(&output_stream));
}

TEST(IndexRepositoryTest, RepositoryPathsAreUnderHomeDirectory) {
  ScopedTempDir temp_dir("minisearch_index_repository_home");
  ScopedHome scoped_home(temp_dir.path());

  EXPECT_EQ(IndexRepository::dataRoot(), temp_dir.path() / ".minisearch");
  EXPECT_EQ(IndexRepository::indexesRoot(),
            temp_dir.path() / ".minisearch" / "indexes");
  EXPECT_EQ(IndexRepository::currentPointerFile(),
            temp_dir.path() / ".minisearch" / "current.pb");
  EXPECT_EQ(IndexRepository::indexCatalogFile(),
            temp_dir.path() / ".minisearch" / "indexes.pb");
}

TEST(IndexRepositoryTest, IndexFileForPathUsesIndexesDirectoryAndPbExtension) {
  ScopedTempDir temp_dir("minisearch_index_repository_index_file");
  ScopedHome scoped_home(temp_dir.path());

  const auto index_file =
      IndexRepository::indexFileForPath(temp_dir.path() / "src");

  EXPECT_EQ(index_file.parent_path(), IndexRepository::indexesRoot());
  EXPECT_EQ(index_file.extension(), std::filesystem::path(".pb"));
  EXPECT_EQ(index_file.filename().string().size(), 19U);
}

TEST(IndexRepositoryTest, SaveAndLoadCurrentIndexRoundTripsPointer) {
  ScopedTempDir temp_dir("minisearch_index_repository_current");
  ScopedHome scoped_home(temp_dir.path());
  const auto index_file = temp_dir.path() / "custom.pb";

  IndexRepository::saveCurrentIndex("root-path", index_file);
  const auto current_index = IndexRepository::loadCurrentIndex();

  EXPECT_EQ(current_index.rootPath, "root-path");
  EXPECT_EQ(current_index.indexFile, IndexRepository::canonicalKey(index_file));
}

TEST(IndexRepositoryTest, SaveCurrentIndexWritesCurrentSchemaVersion) {
  ScopedTempDir temp_dir("minisearch_index_repository_schema");
  ScopedHome scoped_home(temp_dir.path());
  const auto index_file = temp_dir.path() / "custom.pb";

  IndexRepository::saveCurrentIndex("root-path", index_file);
  const proto::CurrentIndex current_index = ReadProtoCurrentIndex();

  EXPECT_EQ(current_index.version(), IndexSchema::currentIndexPointerVersion());
  EXPECT_EQ(current_index.root_path(), "root-path");
  EXPECT_EQ(current_index.index_file(),
            IndexRepository::canonicalKey(index_file));
}

TEST(IndexRepositoryTest, SaveCurrentIndexUpdatesRecentIndexCatalog) {
  ScopedTempDir temp_dir("minisearch_index_repository_catalog");
  ScopedHome scoped_home(temp_dir.path());
  const auto first_index_file = temp_dir.path() / "first.pb";
  const auto second_index_file = temp_dir.path() / "second.pb";

  IndexRepository::saveCurrentIndex("first-root", first_index_file);
  IndexRepository::saveCurrentIndex("second-root", second_index_file);

  const std::vector<IndexRepository::ManagedIndex> recent_indexes =
      IndexRepository::loadRecentIndexes();
  ASSERT_EQ(recent_indexes.size(), 2U);
  EXPECT_EQ(recent_indexes[0].rootPath, "second-root");
  EXPECT_EQ(recent_indexes[0].indexFile,
            IndexRepository::canonicalKey(second_index_file));
  EXPECT_EQ(recent_indexes[1].rootPath, "first-root");
  EXPECT_EQ(recent_indexes[1].indexFile,
            IndexRepository::canonicalKey(first_index_file));
}

TEST(IndexRepositoryTest, SaveCurrentIndexMovesExistingRecentIndexToFront) {
  ScopedTempDir temp_dir("minisearch_index_repository_catalog_dedupe");
  ScopedHome scoped_home(temp_dir.path());
  const auto first_index_file = temp_dir.path() / "first.pb";
  const auto second_index_file = temp_dir.path() / "second.pb";

  IndexRepository::saveCurrentIndex("first-root", first_index_file);
  IndexRepository::saveCurrentIndex("second-root", second_index_file);
  IndexRepository::saveCurrentIndex("first-root", first_index_file);

  const std::vector<IndexRepository::ManagedIndex> recent_indexes =
      IndexRepository::loadRecentIndexes();
  ASSERT_EQ(recent_indexes.size(), 2U);
  EXPECT_EQ(recent_indexes[0].rootPath, "first-root");
  EXPECT_EQ(recent_indexes[1].rootPath, "second-root");
}

TEST(IndexRepositoryTest, LoadRecentIndexesFallsBackToCurrentPointer) {
  ScopedTempDir temp_dir("minisearch_index_repository_catalog_fallback");
  ScopedHome scoped_home(temp_dir.path());

  proto::CurrentIndex current_index;
  current_index.set_version(IndexSchema::currentIndexPointerVersion());
  current_index.set_root_path("legacy-root");
  current_index.set_index_file("legacy.pb");
  WriteProtoCurrentIndex(current_index);

  const std::vector<IndexRepository::ManagedIndex> recent_indexes =
      IndexRepository::loadRecentIndexes();
  ASSERT_EQ(recent_indexes.size(), 1U);
  EXPECT_EQ(recent_indexes[0].rootPath, "legacy-root");
  EXPECT_EQ(recent_indexes[0].indexFile, std::filesystem::path("legacy.pb"));
}

TEST(IndexRepositoryTest, LoadRecentIndexesThrowsForFutureCatalogVersion) {
  ScopedTempDir temp_dir("minisearch_index_repository_catalog_future");
  ScopedHome scoped_home(temp_dir.path());

  proto::IndexCatalog index_catalog;
  index_catalog.set_version(IndexSchema::currentIndexCatalogVersion() + 1);
  proto::IndexCatalogEntry* catalog_entry = index_catalog.add_entries();
  catalog_entry->set_root_path("root-path");
  catalog_entry->set_index_file("index.pb");
  WriteProtoIndexCatalog(index_catalog);

  EXPECT_THROW(IndexRepository::loadRecentIndexes(), std::runtime_error);
}

TEST(IndexRepositoryTest, LoadCurrentIndexThrowsForFutureSchemaVersion) {
  ScopedTempDir temp_dir("minisearch_index_repository_future");
  ScopedHome scoped_home(temp_dir.path());

  proto::CurrentIndex current_index;
  current_index.set_version(IndexSchema::currentIndexPointerVersion() + 1);
  current_index.set_root_path("root-path");
  current_index.set_index_file("index.pb");
  WriteProtoCurrentIndex(current_index);

  EXPECT_THROW(IndexRepository::loadCurrentIndex(), std::runtime_error);
}

}  // namespace
