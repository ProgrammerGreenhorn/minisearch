#include "minisearch/index/InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_set>

namespace minisearch::index {

auto InvertedIndex::clear() -> void {
  records_.clear();
  postings_.clear();
}

auto InvertedIndex::addRecord(FileRecord record) -> DocumentId {
  record.id = static_cast<DocumentId>(records_.size());
  records_.push_back(std::move(record));
  return records_.back().id;
}

auto InvertedIndex::addTerms(DocumentId id,
                             const std::vector<std::string>& terms) -> void {
  std::unordered_set<std::string> uniqueTerms(terms.begin(), terms.end());
  for (const auto& term : uniqueTerms) {
    postings_[term].push_back(id);
  }
}

auto InvertedIndex::replace(
    std::vector<FileRecord> records,
    std::unordered_map<std::string, PostingList> postings) -> void {
  records_ = std::move(records);
  postings_ = std::move(postings);
}

auto InvertedIndex::findByName(const std::string& query, std::size_t limit)
    const -> std::vector<FileRecord> {
  std::vector<FileRecord> result;
  for (const auto& record : records_) {
    if (containsCaseInsensitive(record.path.filename().string(), query) ||
        containsCaseInsensitive(record.path.string(), query)) {
      result.push_back(record);
      if (result.size() >= limit) {
        break;
      }
    }
  }
  return result;
}

auto InvertedIndex::findByTerms(const std::vector<std::string>& terms,
                                std::size_t limit) const
    -> std::vector<FileRecord> {
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

auto InvertedIndex::records() const -> const std::vector<FileRecord>& {
  return records_;
}

auto InvertedIndex::postings() const
    -> const std::unordered_map<std::string, PostingList>& {
  return postings_;
}

auto InvertedIndex::fileCount() const -> std::size_t { return records_.size(); }

auto InvertedIndex::indexedTextFileCount() const -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      records_.begin(), records_.end(),
      [](const auto& record) -> bool { return record.textIndexed; }));
}

auto InvertedIndex::termCount() const -> std::size_t {
  return postings_.size();
}

auto InvertedIndex::recordById(DocumentId id) const -> const FileRecord* {
  if (id >= records_.size()) {
    return nullptr;
  }
  return &records_[id];
}

auto InvertedIndex::lower(std::string value) -> std::string {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) -> char {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

auto InvertedIndex::containsCaseInsensitive(const std::string& text,
                                            const std::string& query) -> bool {
  return lower(text).find(lower(query)) != std::string::npos;
}

}  // namespace minisearch::index
