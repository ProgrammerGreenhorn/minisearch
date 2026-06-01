#pragma once

#include "minisearch/index/FileRecord.hpp"
#include "minisearch/index/InvertedIndex.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace minisearch::search {

class SearchEngine {
public:
    explicit SearchEngine(const index::InvertedIndex& index);

    std::vector<index::FileRecord> findByName(const std::string& query,
                                              std::size_t limit = 50) const;
    std::vector<index::FileRecord> grep(const std::string& query,
                                        std::size_t limit = 50) const;

    static std::string formatRecord(const index::FileRecord& record);

private:
    const index::InvertedIndex& index_;
};

} // namespace minisearch::search

