#pragma once

#include <filesystem>

#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::index {

class IndexStorage {
 public:
  auto save(const std::filesystem::path &path, const InvertedIndex &index,
            const std::filesystem::path &rootPath) const -> void;

  auto load(const std::filesystem::path &path) const -> InvertedIndex;
};

}  // namespace minisearch::index
