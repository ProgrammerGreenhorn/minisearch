#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include "minisearch/index/IndexRepository.hpp"

namespace {

using minisearch::index::IndexRepository;

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

class ScopedHome {
 public:
  explicit ScopedHome(const std::filesystem::path& home) {
    const char* old_home = std::getenv("HOME");
    if (old_home != nullptr) {
      old_home_ = old_home;
      had_old_home_ = true;
    }
    setenv("HOME", home.string().c_str(), 1);
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

TEST(IndexRepositoryTest, RepositoryPathsAreUnderHomeDirectory) {
  ScopedTempDir temp("minisearch_index_repository_home");
  ScopedHome home(temp.path());

  EXPECT_EQ(IndexRepository::dataRoot(), temp.path() / ".minisearch");
  EXPECT_EQ(IndexRepository::indexesRoot(),
            temp.path() / ".minisearch" / "indexes");
  EXPECT_EQ(IndexRepository::currentPointerFile(),
            temp.path() / ".minisearch" / "current.pb");
}

TEST(IndexRepositoryTest, IndexFileForPathUsesIndexesDirectoryAndPbExtension) {
  ScopedTempDir temp("minisearch_index_repository_index_file");
  ScopedHome home(temp.path());

  const auto index_file =
      IndexRepository::indexFileForPath(temp.path() / "src");

  EXPECT_EQ(index_file.parent_path(), IndexRepository::indexesRoot());
  EXPECT_EQ(index_file.extension(), std::filesystem::path(".pb"));
  EXPECT_EQ(index_file.filename().string().size(), 19U);
}

TEST(IndexRepositoryTest, SaveAndLoadCurrentIndexRoundTripsPointer) {
  ScopedTempDir temp("minisearch_index_repository_current");
  ScopedHome home(temp.path());
  const auto index_file = temp.path() / "custom.pb";

  IndexRepository::saveCurrentIndex("root-path", index_file);
  const auto current = IndexRepository::loadCurrentIndex();

  EXPECT_EQ(current.rootPath, "root-path");
  EXPECT_EQ(current.indexFile, IndexRepository::canonicalKey(index_file));
}

}  // namespace
