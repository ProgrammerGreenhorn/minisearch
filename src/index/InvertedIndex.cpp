#include "minisearch/index/InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_set>

namespace minisearch::index {

void InvertedIndex::clear()
{
    records_.clear();
    postings_.clear();
}

InvertedIndex::DocumentId InvertedIndex::addRecord(FileRecord record)
{
    record.id = static_cast<DocumentId>(records_.size());
    records_.push_back(std::move(record));
    return records_.back().id;
}

void InvertedIndex::addTerms(DocumentId id, const std::vector<std::string>& terms)
{
    std::unordered_set<std::string> uniqueTerms(terms.begin(), terms.end());
    for (const auto& term : uniqueTerms) {
        postings_[term].push_back(id);
    }
}

void InvertedIndex::replace(std::vector<FileRecord> records,
                            std::unordered_map<std::string, PostingList> postings)
{
    records_ = std::move(records);
    postings_ = std::move(postings);
}

std::vector<FileRecord> InvertedIndex::findByName(const std::string& query,
                                                  std::size_t limit) const
{
    std::vector<FileRecord> result;
    for (const auto& record : records_) {
        if (containsCaseInsensitive(record.path.filename().string(), query)
            || containsCaseInsensitive(record.path.string(), query)) {
            result.push_back(record);
            if (result.size() >= limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<FileRecord> InvertedIndex::findByTerms(const std::vector<std::string>& terms,
                                                   std::size_t limit) const
{
    if (terms.empty()) {
        return {};
    }

    auto first = postings_.find(terms.front());
    if (first == postings_.end()) {
        return {};
    }

    std::vector<DocumentId> matches = first->second;
    for (std::size_t i = 1; i < terms.size(); ++i) {
        auto posting = postings_.find(terms[i]);
        if (posting == postings_.end()) {
            return {};
        }

        std::vector<DocumentId> intersection;
        std::set_intersection(matches.begin(), matches.end(),
                              posting->second.begin(), posting->second.end(),
                              std::back_inserter(intersection));
        matches = std::move(intersection);
        if (matches.empty()) {
            return {};
        }
    }

    std::vector<FileRecord> result;
    for (const auto id : matches) {
        if (const auto* record = recordById(id)) {
            result.push_back(*record);
            if (result.size() >= limit) {
                break;
            }
        }
    }

    return result;
}

const std::vector<FileRecord>& InvertedIndex::records() const
{
    return records_;
}

const std::unordered_map<std::string, InvertedIndex::PostingList>& InvertedIndex::postings() const
{
    return postings_;
}

std::size_t InvertedIndex::fileCount() const
{
    return records_.size();
}

std::size_t InvertedIndex::indexedTextFileCount() const
{
    return static_cast<std::size_t>(
        std::count_if(records_.begin(), records_.end(), [](const auto& record) {
            return record.textIndexed;
        }));
}

std::size_t InvertedIndex::termCount() const
{
    return postings_.size();
}

const FileRecord* InvertedIndex::recordById(DocumentId id) const
{
    if (id >= records_.size()) {
        return nullptr;
    }
    return &records_[id];
}

std::string InvertedIndex::lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool InvertedIndex::containsCaseInsensitive(const std::string& text, const std::string& query)
{
    return lower(text).find(lower(query)) != std::string::npos;
}

} // namespace minisearch::index

