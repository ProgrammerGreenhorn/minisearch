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

auto WriteFile(const std::filesystem::path& file_path,
               const std::string& contents) -> void {
  std::filesystem::create_directories(file_path.parent_path());
  std::ofstream output_stream(file_path);
  ASSERT_TRUE(output_stream);
  output_stream << contents;
}

auto SortByPath(std::vector<FileRecord> file_records)
    -> std::vector<FileRecord> {
  std::sort(file_records.begin(), file_records.end(),
            [](const FileRecord& left_record,
               const FileRecord& right_record) -> bool {
              return left_record.path.string() < right_record.path.string();
            });
  return file_records;
}

TEST(FileScannerTest, ScansRegularTextFile) {
  ScopedTempDir temp_dir("minisearch_file_scanner_file");
  const auto target_file = temp_dir.path() / "README.MD";
  WriteFile(target_file, "hello\n");

  FileScanner file_scanner;
  const auto file_records = file_scanner.scan(target_file);

  ASSERT_EQ(file_records.size(), 1U);
  EXPECT_EQ(file_records[0].path,
            std::filesystem::absolute(target_file).lexically_normal());
  EXPECT_EQ(file_records[0].size, 6U);
  EXPECT_TRUE(file_records[0].textIndexed);
  EXPECT_NE(file_records[0].contentHash, 0U);
}

TEST(FileScannerTest, ScansDirectoryAndSkipsExcludedNames) {
  ScopedTempDir temp_dir("minisearch_file_scanner_dir");
  WriteFile(temp_dir.path() / "src" / "main.cpp", "int main() {}\n");
  WriteFile(temp_dir.path() / "data.bin", "binary");
  WriteFile(temp_dir.path() / "build" / "generated.cpp", "skip me");

  FileScanner file_scanner;
  const auto file_records = SortByPath(file_scanner.scan(temp_dir.path()));

  ASSERT_EQ(file_records.size(), 2U);
  EXPECT_EQ(file_records[0].path.filename(), std::filesystem::path("data.bin"));
  EXPECT_TRUE(file_records[0].textIndexed);
  EXPECT_NE(file_records[0].contentHash, 0U);
  EXPECT_EQ(file_records[1].path.filename(), std::filesystem::path("main.cpp"));
  EXPECT_TRUE(file_records[1].textIndexed);
}

TEST(FileScannerTest, HonorsMaxTextFileBytes) {
  ScopedTempDir temp_dir("minisearch_file_scanner_size");
  const auto target_file = temp_dir.path() / "large.txt";
  WriteFile(target_file, "0123456789");

  FileScanner::Options scanner_options;
  scanner_options.maxTextFileBytes = 4;
  FileScanner file_scanner(scanner_options);
  const auto file_records = file_scanner.scan(target_file);

  ASSERT_EQ(file_records.size(), 1U);
  EXPECT_FALSE(file_records[0].textIndexed);
  EXPECT_EQ(file_records[0].contentHash, 0U);
}

TEST(FileScannerTest, ThrowsWhenPathDoesNotExist) {
  ScopedTempDir temp_dir("minisearch_file_scanner_missing");
  const auto missing_path = temp_dir.path() / "missing";

  FileScanner file_scanner;
  EXPECT_THROW(file_scanner.scan(missing_path), std::runtime_error);
}

}  // namespace
