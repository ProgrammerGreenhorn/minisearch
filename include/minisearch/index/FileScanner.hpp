#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/FileRecord.hpp"

namespace minisearch::index {

class FileScanner {
 public:
  struct Options {
    std::vector<std::string> textExtensions{
        ".txt",  ".md",  ".markdown", ".cpp", ".cc",   ".cxx",
        ".c",    ".hpp", ".hh",       ".hxx", ".h",    ".json",
        ".yaml", ".yml", ".toml",     ".ini", ".cmake"};
    std::vector<std::string> excludedNames{".git",
                                           "build",
                                           "cmake-build-debug",
                                           "cmake-build-release",
                                           "node_modules",
                                           ".idea",
                                           ".vscode"};
    std::uintmax_t maxTextFileBytes = 1024 * 1024;
  };

  FileScanner();

  explicit FileScanner(Options options);

  auto scan(const std::filesystem::path& root) const -> std::vector<FileRecord>;

 private:
  auto shouldSkip(const std::filesystem::directory_entry& entry) const -> bool;
  auto shouldIndexText(const std::filesystem::path& path,
                       std::uintmax_t size) const -> bool;
  static auto toTimeT(std::filesystem::file_time_type value) -> std::time_t;
  static auto lower(std::string value) -> std::string;

  Options options_;
};

}  // namespace minisearch::index
