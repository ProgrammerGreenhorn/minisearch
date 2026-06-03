#include "minisearch/index/InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_map>

namespace minisearch::index {

namespace {

auto sortedUniqueLines(std::vector<std::uint32_t> lines)
    -> std::vector<std::uint32_t> {
  std::sort(lines.begin(), lines.end());
  lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
  return lines;
}

auto findLinePosting(const InvertedIndex::LinePostingList& postings,
                     InvertedIndex::DocumentId id)
    -> const InvertedIndex::LinePosting* {
  const auto found =
      std::find_if(postings.begin(), postings.end(),
                   [id](const InvertedIndex::LinePosting& posting) -> bool {
                     return posting.documentId == id;
                   });
  return found == postings.end() ? nullptr : &*found;
}

auto upsertLinePosting(InvertedIndex::LinePostingList& postings,
                       InvertedIndex::DocumentId id,
                       std::vector<std::uint32_t> lines) -> void {
  auto found =
      std::find_if(postings.begin(), postings.end(),
                   [id](const InvertedIndex::LinePosting& posting) -> bool {
                     return posting.documentId == id;
                   });
  if (found == postings.end()) {
    postings.push_back({id, sortedUniqueLines(std::move(lines))});
    return;
  }

  found->lines.insert(found->lines.end(), lines.begin(), lines.end());
  found->lines = sortedUniqueLines(std::move(found->lines));
}

}  // namespace

auto InvertedIndex::clear() -> void {
  records_.clear();
  linePostings_.clear();
}

auto InvertedIndex::addRecord(FileRecord record) -> DocumentId {
  record.id = static_cast<DocumentId>(records_.size());
  records_.push_back(std::move(record));
  return records_.back().id;
}

auto InvertedIndex::addTermOccurrences(
    DocumentId id, const std::vector<ParsedTerm>& terms) -> void {
  std::unordered_map<std::string, std::vector<std::uint32_t>> linesByTerm;
  for (const auto& parsedTerm : terms) {
    if (parsedTerm.term.empty()) {
      continue;
    }

    linesByTerm[parsedTerm.term].push_back(parsedTerm.line);
  }

  for (auto& [term, lines] : linesByTerm) {
    upsertLinePosting(linePostings_[term], id, std::move(lines));
  }
}

auto InvertedIndex::replace(
    std::vector<FileRecord> records,
    std::unordered_map<std::string, LinePostingList> linePostings) -> void {
  records_ = std::move(records);
  linePostings_ = std::move(linePostings);
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
    -> std::vector<LineMatch> {
  if (terms.empty()) {
    return {};
  }

  auto first = linePostings_.find(terms.front());
  if (first == linePostings_.end()) {
    return {};
  }

  std::vector<LineMatch> candidates;
  for (const auto& posting : first->second) {
    const FileRecord* record = recordById(posting.documentId);
    if (record != nullptr && !posting.lines.empty()) {
      candidates.push_back({*record, posting.lines});
    }
  }

  for (std::size_t i = 1; i < terms.size(); ++i) {
    auto termPostings = linePostings_.find(terms[i]);
    if (termPostings == linePostings_.end()) {
      return {};
    }

    std::vector<LineMatch> nextCandidates;
    for (auto& candidate : candidates) {
      const LinePosting* posting =
          findLinePosting(termPostings->second, candidate.record.id);
      if (posting == nullptr) {
        continue;
      }

      std::vector<std::uint32_t> lines;
      std::set_intersection(candidate.lines.begin(), candidate.lines.end(),
                            posting->lines.begin(), posting->lines.end(),
                            std::back_inserter(lines));
      if (!lines.empty()) {
        candidate.lines = std::move(lines);
        nextCandidates.push_back(std::move(candidate));
      }
    }

    candidates = std::move(nextCandidates);
    if (candidates.empty()) {
      return {};
    }
  }

  std::vector<LineMatch> result;
  for (auto& candidate : candidates) {
    result.push_back(std::move(candidate));
    if (result.size() >= limit) {
      break;
    }
  }
  return result;
}

auto InvertedIndex::records() const -> const std::vector<FileRecord>& {
  return records_;
}

auto InvertedIndex::linePostings() const
    -> const std::unordered_map<std::string, LinePostingList>& {
  return linePostings_;
}

auto InvertedIndex::fileCount() const -> std::size_t { return records_.size(); }

auto InvertedIndex::indexedTextFileCount() const -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      records_.begin(), records_.end(),
      [](const FileRecord& record) -> bool { return record.textIndexed; }));
}

auto InvertedIndex::termCount() const -> std::size_t {
  return linePostings_.size();
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
