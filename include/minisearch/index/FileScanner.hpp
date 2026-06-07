#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"

namespace minisearch::index {

/**
 * @brief Scan paths into file records that can be indexed.
 *
 * FileScanner walks files and directories, filters out excluded entries, and
 * decides which files should also have text content parsed.
 */
class FileScanner {
 public:
  struct Options {
    std::vector<std::string> excludedNames;
    std::uintmax_t maxTextFileBytes = 1024 * 1024;
    std::size_t textProbeBytes = 8192;
    double binaryControlRatio = 0.05;
  };

  /**
   * @brief Create a scanner with default options.
   */
  FileScanner();

  /**
   * @brief Create a scanner with explicit scanning options.
   *
   * @param scanner_options Exclusion, size-limit, and text-detection options.
   */
  explicit FileScanner(Options scanner_options);

  /**
   * @brief Scan a file or directory tree into file records.
   *
   * @param root_path File or directory path to scan.
   * @return File records found under the root path.
   */
  auto scan(const std::filesystem::path& root_path) const
      -> std::vector<FileRecord>;

 private:
  /**
   * @brief Check whether a directory entry should be skipped.
   *
   * @param directory_entry Directory entry being considered.
   * @return True if the entry name is excluded.
   */
  auto shouldSkip(const std::filesystem::directory_entry& directory_entry) const
      -> bool;

  /**
   * @brief Check whether a file should have text content indexed.
   *
   * @param file_path File path whose contents should be probed.
   * @param file_size File size in bytes.
   * @return True if the file is small enough and appears to be text.
   */
  auto shouldIndexText(const std::filesystem::path& file_path,
                       std::uintmax_t file_size) const -> bool;

  /**
   * @brief Convert a filesystem clock timestamp to time_t.
   *
   * @param file_time Filesystem timestamp to convert.
   * @return Converted system time value.
   */
  static auto toTimeT(std::filesystem::file_time_type file_time) -> std::time_t;

  Options options_;
};

}  // namespace minisearch::index
