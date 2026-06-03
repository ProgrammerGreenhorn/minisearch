#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::index {

class IndexBuilder {
 public:
  struct Options {
    std::filesystem::path targetPath;
    std::filesystem::path indexFile;
    std::size_t threads = 0;
  };

  struct Result {
    InvertedIndex index;
    std::string rootPath;
    std::filesystem::path indexFile;
    std::size_t reusedTextFiles = 0;
    std::size_t parsedTextFiles = 0;
  };

  auto build(const Options& options) const -> Result;
};

}  // namespace minisearch::index
