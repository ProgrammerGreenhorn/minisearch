#pragma once

#include <filesystem>
#include <string>

#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::index {

class IndexStorage {
 public:
  struct Metadata {
    std::string rootPath;
  };

  /**
   * @brief Save an inverted index to a protobuf file.
   *
   * @param index_file Output protobuf file path.
   * @param search_index Index data to serialize.
   * @param root_path Canonical root path represented by the index.
   */
  auto save(const std::filesystem::path &index_file,
            const InvertedIndex &search_index,
            const std::filesystem::path &root_path) const -> void;

  /**
   * @brief Load an inverted index from a protobuf file.
   *
   * @param index_file Input protobuf file path.
   * @return Deserialized inverted index.
   */
  auto load(const std::filesystem::path &index_file) const -> InvertedIndex;

  /**
   * @brief Load index metadata without materializing the inverted index.
   *
   * @param index_file Input protobuf file path.
   * @return Metadata stored in the index file.
   */
  auto loadMetadata(const std::filesystem::path &index_file) const -> Metadata;
};

}  // namespace minisearch::index
