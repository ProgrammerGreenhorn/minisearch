#pragma once

#include "minisearch/index/InvertedIndex.hpp"

#include <filesystem>

namespace minisearch::index {

class IndexStorage {
public:
    void save(const std::filesystem::path& path, const InvertedIndex& index) const;
    InvertedIndex load(const std::filesystem::path& path) const;
};

} // namespace minisearch::index

