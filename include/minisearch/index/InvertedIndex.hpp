#pragma once

#include "minisearch/index/FileRecord.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace minisearch::index {

class InvertedIndex {
public:
    using DocumentId = FileRecord::Id;
    using PostingList = std::vector<DocumentId>;

    void clear();

    DocumentId addRecord(FileRecord record);
    void addTerms(DocumentId id, const std::vector<std::string>& terms);
    void replace(std::vector<FileRecord> records,
                 std::unordered_map<std::string, PostingList> postings);

    std::vector<FileRecord> findByName(const std::string& query, std::size_t limit) const;
    std::vector<FileRecord> findByTerms(const std::vector<std::string>& terms,
                                        std::size_t limit) const;

    const std::vector<FileRecord>& records() const;
    const std::unordered_map<std::string, PostingList>& postings() const;

    std::size_t fileCount() const;
    std::size_t indexedTextFileCount() const;
    std::size_t termCount() const;

private:
    const FileRecord* recordById(DocumentId id) const;
    static std::string lower(std::string value);
    static bool containsCaseInsensitive(const std::string& text, const std::string& query);

    std::vector<FileRecord> records_;
    std::unordered_map<std::string, PostingList> postings_;
};

} // namespace minisearch::index

