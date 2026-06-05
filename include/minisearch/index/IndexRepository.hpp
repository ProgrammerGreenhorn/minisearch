#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/IndexCatalogStorage.hpp"

namespace minisearch::index {

/**
 * @brief Owns the MiniSearch index metadata paths and persistence helpers.
 *
 * This class resolves the data-directory layout under ~/.minisearch and
 * reads/writes the protobuf files that track the current index pointer and
 * recent index catalog.
 */
class IndexRepository {
 public:
  struct CurrentIndex {
    std::string rootPath;
    std::filesystem::path indexFile;
  };

  using ManagedIndex = minisearch::index::ManagedIndex;

  /**
   * @brief Resolve the MiniSearch data directory.
   *
   * @return Path to the user data directory.
   */
  static auto dataRoot() -> std::filesystem::path;

  /**
   * @brief Resolve the directory that stores index files.
   *
   * @return Path to the index storage directory.
   */
  static auto indexesRoot() -> std::filesystem::path;

  /**
   * @brief Resolve the current-index pointer file.
   *
   * @return Path to the current index pointer protobuf file.
   */
  static auto currentPointerFile() -> std::filesystem::path;

  /**
   * @brief Resolve the recent-index catalog file.
   *
   * @return Path to the index catalog protobuf file.
   */
  static auto indexCatalogFile() -> std::filesystem::path;

  /**
   * @brief Generate a stable string key for an indexed path.
   *
   * @param indexed_path Path to normalize and canonicalize where possible.
   * @return Stable string key for path-based indexing metadata.
   */
  static auto canonicalKey(const std::filesystem::path &indexed_path)
      -> std::string;

  /**
   * @brief Resolve the default index file path for an indexed path.
   *
   * @param indexed_path Path whose canonical key is hashed.
   * @return Protobuf index file path under the MiniSearch data directory.
   */
  static auto indexFileForPath(const std::filesystem::path &indexed_path)
      -> std::filesystem::path;

  /**
   * @brief Save the pointer to the most recently built index.
   *
   * @param root_path Canonical root path represented by the index.
   * @param index_file Protobuf index file to open by default later.
   */
  static auto saveCurrentIndex(const std::string &root_path,
                               const std::filesystem::path &index_file) -> void;

  /**
   * @brief Load the pointer to the most recently built index.
   *
   * @return Current index root path and protobuf file path.
   */
  static auto loadCurrentIndex() -> CurrentIndex;

  /**
   * @brief Load the recently built index catalog.
   *
   * @return Recent indexes ordered from newest to oldest.
   */
  static auto loadRecentIndexes() -> std::vector<ManagedIndex>;
};

}  // namespace minisearch::index
