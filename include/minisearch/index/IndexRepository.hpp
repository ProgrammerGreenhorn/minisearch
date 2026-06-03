#pragma once

#include <filesystem>
#include <string>

namespace minisearch::index {

class IndexRepository {
 public:
  struct CurrentIndex {
    std::string rootPath;
    std::filesystem::path indexFile;
  };

  static auto dataRoot() -> std::filesystem::path;

  static auto indexesRoot() -> std::filesystem::path;

  static auto currentPointerFile() -> std::filesystem::path;

  /* generates a stable string key for the given path */
  static auto canonicalKey(const std::filesystem::path &indexedPath)
      -> std::string;

  static auto indexFileForPath(const std::filesystem::path &indexedPath)
      -> std::filesystem::path;

  static auto saveCurrentIndex(const std::string &rootPath,
                               const std::filesystem::path &indexFile) -> void;

  static auto loadCurrentIndex() -> CurrentIndex;
};

}  // namespace minisearch::index
