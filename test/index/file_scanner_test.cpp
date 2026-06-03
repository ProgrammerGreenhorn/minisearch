#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "minisearch/index/FileScanner.hpp"

namespace {

using minisearch::index::FileRecord;
using minisearch::index::FileScanner;

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

auto WriteFile(const std::filesystem::path& path,
               const std::string& contents) -> void {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  ASSERT_TRUE(output);
  output << contents;
}

auto SortByPath(std::vector<FileRecord> records) -> std::vector<FileRecord> {
  std::sort(records.begin(), records.end(),
            [](const FileRecord& left, const FileRecord& right) -> bool {
              return left.path.string() < right.path.string();
            });
  return records;
}

TEST(FileScannerTest, ScansRegularTextFile) {
  ScopedTempDir temp("minisearch_file_scanner_file");
  const auto file = temp.path() / "README.MD";
  WriteFile(file, "hello\n");

  FileScanner scanner;
  const auto records = scanner.scan(file);

  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records[0].path,
            std::filesystem::absolute(file).lexically_normal());
  EXPECT_EQ(records[0].size, 6U);
  EXPECT_TRUE(records[0].textIndexed);
  EXPECT_NE(records[0].contentHash, 0U);
}

TEST(FileScannerTest, ScansDirectoryAndSkipsExcludedNames) {
  ScopedTempDir temp("minisearch_file_scanner_dir");
  WriteFile(temp.path() / "src" / "main.cpp", "int main() {}\n");
  WriteFile(temp.path() / "data.bin", "binary");
  WriteFile(temp.path() / "build" / "generated.cpp", "skip me");

  FileScanner scanner;
  const auto records = SortByPath(scanner.scan(temp.path()));

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[0].path.filename(), std::filesystem::path("data.bin"));
  EXPECT_FALSE(records[0].textIndexed);
  EXPECT_EQ(records[0].contentHash, 0U);
  EXPECT_EQ(records[1].path.filename(), std::filesystem::path("main.cpp"));
  EXPECT_TRUE(records[1].textIndexed);
}

TEST(FileScannerTest, HonorsMaxTextFileBytes) {
  ScopedTempDir temp("minisearch_file_scanner_size");
  const auto file = temp.path() / "large.txt";
  WriteFile(file, "0123456789");

  FileScanner::Options options;
  options.maxTextFileBytes = 4;
  FileScanner scanner(options);
  const auto records = scanner.scan(file);

  ASSERT_EQ(records.size(), 1U);
  EXPECT_FALSE(records[0].textIndexed);
  EXPECT_EQ(records[0].contentHash, 0U);
}

TEST(FileScannerTest, ThrowsWhenPathDoesNotExist) {
  ScopedTempDir temp("minisearch_file_scanner_missing");
  const auto missing = temp.path() / "missing";

  FileScanner scanner;
  EXPECT_THROW(scanner.scan(missing), std::runtime_error);
}

}  // namespace
