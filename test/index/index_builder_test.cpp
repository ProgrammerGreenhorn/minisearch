#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "minisearch/index/IndexBuilder.hpp"
#include "minisearch/index/IndexRepository.hpp"

namespace {

using minisearch::index::IndexBuilder;
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

auto WriteFile(const std::filesystem::path& path,
               const std::string& contents) -> void {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  ASSERT_TRUE(output);
  output << contents;
}

TEST(IndexBuilderTest, BuildsSearchableIndexAndSavesCurrentPointer) {
  ScopedTempDir home("minisearch_index_builder_home");
  ScopedHome scoped_home(home.path());
  ScopedTempDir root("minisearch_index_builder_root");
  WriteFile(root.path() / "doc.txt", "Alpha beta alpha");
  WriteFile(root.path() / "image.bin", "binary");

  const auto index_file = home.path() / "custom" / "index.pb";
  IndexBuilder builder;
  const auto result = builder.build({root.path(), index_file, 1});

  EXPECT_EQ(result.rootPath, IndexRepository::canonicalKey(root.path()));
  EXPECT_EQ(result.indexFile, index_file);
  EXPECT_EQ(result.parsedTextFiles, 1U);
  EXPECT_EQ(result.reusedTextFiles, 0U);
  EXPECT_EQ(result.index.fileCount(), 2U);
  EXPECT_EQ(result.index.indexedTextFileCount(), 1U);
  const auto matches = result.index.findByTerms({"alpha", "beta"}, 10);
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].lines, std::vector<std::uint32_t>({1}));
  EXPECT_TRUE(std::filesystem::exists(index_file));

  const auto current = IndexRepository::loadCurrentIndex();
  EXPECT_EQ(current.rootPath, result.rootPath);
  EXPECT_EQ(current.indexFile, IndexRepository::canonicalKey(index_file));
}

TEST(IndexBuilderTest, ReusesUnchangedTextFilesFromPreviousIndex) {
  ScopedTempDir home("minisearch_index_builder_reuse_home");
  ScopedHome scoped_home(home.path());
  ScopedTempDir root("minisearch_index_builder_reuse_root");
  WriteFile(root.path() / "doc.txt", "Alpha beta");

  const auto index_file = home.path() / "index.pb";
  IndexBuilder builder;
  const IndexBuilder::Options options{root.path(), index_file, 1};

  const auto first = builder.build(options);
  const auto second = builder.build(options);

  EXPECT_EQ(first.parsedTextFiles, 1U);
  EXPECT_EQ(first.reusedTextFiles, 0U);
  EXPECT_EQ(second.parsedTextFiles, 0U);
  EXPECT_EQ(second.reusedTextFiles, 1U);
  const auto matches = second.index.findByTerms({"alpha", "beta"}, 10);
  ASSERT_EQ(matches.size(), 1U);
  EXPECT_EQ(matches[0].lines, std::vector<std::uint32_t>({1}));
}

}  // namespace
