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

auto WriteFile(const std::filesystem::path& file_path,
               const std::string& contents) -> void {
  std::filesystem::create_directories(file_path.parent_path());
  std::ofstream output_stream(file_path);
  ASSERT_TRUE(output_stream);
  output_stream << contents;
}

TEST(IndexBuilderTest, BuildsSearchableIndexAndSavesCurrentPointer) {
  ScopedTempDir home_dir("minisearch_index_builder_home");
  ScopedHome scoped_home(home_dir.path());
  ScopedTempDir root_dir("minisearch_index_builder_root");
  WriteFile(root_dir.path() / "doc.txt", "Alpha beta alpha");
  WriteFile(root_dir.path() / "image.bin", "binary");

  const auto index_file = home_dir.path() / "custom" / "index.pb";
  IndexBuilder index_builder;
  const auto build_result =
      index_builder.build({root_dir.path(), index_file, 1});

  EXPECT_EQ(build_result.rootPath,
            IndexRepository::canonicalKey(root_dir.path()));
  EXPECT_EQ(build_result.indexFile, index_file);
  EXPECT_EQ(build_result.parsedTextFiles, 1U);
  EXPECT_EQ(build_result.reusedTextFiles, 0U);
  EXPECT_EQ(build_result.index.fileCount(), 2U);
  EXPECT_EQ(build_result.index.indexedTextFileCount(), 1U);
  const auto term_matches =
      build_result.index.findByTerms({"alpha", "beta"}, 10);
  ASSERT_EQ(term_matches.size(), 1U);
  EXPECT_EQ(term_matches[0].lines, std::vector<std::uint32_t>({1}));
  EXPECT_TRUE(std::filesystem::exists(index_file));

  const auto current_index = IndexRepository::loadCurrentIndex();
  EXPECT_EQ(current_index.rootPath, build_result.rootPath);
  EXPECT_EQ(current_index.indexFile, IndexRepository::canonicalKey(index_file));
}

TEST(IndexBuilderTest, ReusesUnchangedTextFilesFromPreviousIndex) {
  ScopedTempDir home_dir("minisearch_index_builder_reuse_home");
  ScopedHome scoped_home(home_dir.path());
  ScopedTempDir root_dir("minisearch_index_builder_reuse_root");
  WriteFile(root_dir.path() / "doc.txt", "Alpha beta");

  const auto index_file = home_dir.path() / "index.pb";
  IndexBuilder index_builder;
  const IndexBuilder::Options build_options{root_dir.path(), index_file, 1};

  const auto first_build_result = index_builder.build(build_options);
  const auto second_build_result = index_builder.build(build_options);

  EXPECT_EQ(first_build_result.parsedTextFiles, 1U);
  EXPECT_EQ(first_build_result.reusedTextFiles, 0U);
  EXPECT_EQ(second_build_result.parsedTextFiles, 0U);
  EXPECT_EQ(second_build_result.reusedTextFiles, 1U);
  const auto term_matches =
      second_build_result.index.findByTerms({"alpha", "beta"}, 10);
  ASSERT_EQ(term_matches.size(), 1U);
  EXPECT_EQ(term_matches[0].lines, std::vector<std::uint32_t>({1}));
}

}  // namespace
