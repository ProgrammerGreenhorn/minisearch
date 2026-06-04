#include "minisearch/index/InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_map>

namespace minisearch::index {

namespace {

auto sortedUniqueLines(std::vector<std::uint32_t> line_numbers)
    -> std::vector<std::uint32_t> {
  std::sort(line_numbers.begin(), line_numbers.end());
  line_numbers.erase(std::unique(line_numbers.begin(), line_numbers.end()),
                     line_numbers.end());
  return line_numbers;
}

auto findLinePosting(const InvertedIndex::LinePostingList& postings,
                     InvertedIndex::DocumentId document_id)
    -> const InvertedIndex::LinePosting* {
  const auto matching_posting_entry = std::find_if(
      postings.begin(), postings.end(),
      [document_id](const InvertedIndex::LinePosting& line_posting) -> bool {
        return line_posting.documentId == document_id;
      });
  return matching_posting_entry == postings.end() ? nullptr
                                                  : &*matching_posting_entry;
}

auto upsertLinePosting(InvertedIndex::LinePostingList& postings,
                       InvertedIndex::DocumentId document_id,
                       std::vector<std::uint32_t> line_numbers) -> void {
  auto matching_posting_entry = std::find_if(
      postings.begin(), postings.end(),
      [document_id](const InvertedIndex::LinePosting& line_posting) -> bool {
        return line_posting.documentId == document_id;
      });
  if (matching_posting_entry == postings.end()) {
    postings.push_back(
        {document_id, sortedUniqueLines(std::move(line_numbers))});
    return;
  }

  matching_posting_entry->lines.insert(matching_posting_entry->lines.end(),
                                       line_numbers.begin(),
                                       line_numbers.end());
  matching_posting_entry->lines =
      sortedUniqueLines(std::move(matching_posting_entry->lines));
}

}  // namespace

auto InvertedIndex::clear() -> void {
  records_.clear();
  linePostings_.clear();
}

auto InvertedIndex::addRecord(FileRecord file_record) -> DocumentId {
  file_record.id = static_cast<DocumentId>(records_.size());
  records_.push_back(std::move(file_record));
  return records_.back().id;
}

auto InvertedIndex::addTermOccurrences(
    DocumentId document_id,
    const std::vector<ParsedTerm>& parsed_terms) -> void {
  std::unordered_map<std::string, std::vector<std::uint32_t>> lines_by_term;
  for (const auto& parsed_term : parsed_terms) {
    if (parsed_term.term.empty()) {
      continue;
    }

    lines_by_term[parsed_term.term].push_back(parsed_term.line);
  }

  for (auto& [term, line_numbers] : lines_by_term) {
    upsertLinePosting(linePostings_[term], document_id,
                      std::move(line_numbers));
  }
}

auto InvertedIndex::replace(
    std::vector<FileRecord> file_records,
    std::unordered_map<std::string, LinePostingList> line_postings) -> void {
  records_ = std::move(file_records);
  linePostings_ = std::move(line_postings);
}

auto InvertedIndex::findByName(const std::string& query_text, std::size_t limit)
    const -> std::vector<FileRecord> {
  std::vector<FileRecord> matching_records;
  for (const auto& file_record : records_) {
    if (containsCaseInsensitive(file_record.path.filename().string(),
                                query_text) ||
        containsCaseInsensitive(file_record.path.string(), query_text)) {
      matching_records.push_back(file_record);
      if (matching_records.size() >= limit) {
        break;
      }
    }
  }
  return matching_records;
}

// Start with the first term's postings, then keep only candidate documents
// whose line numbers also contain every subsequent term.
auto InvertedIndex::findByTerms(const std::vector<std::string>& query_terms,
                                std::size_t limit) const
    -> std::vector<LineMatch> {
  if (query_terms.empty()) {
    return {};
  }

  auto first_term_posting_entry = linePostings_.find(query_terms.front());
  if (first_term_posting_entry == linePostings_.end()) {
    return {};
  }

  std::vector<LineMatch> candidate_matches;
  for (const auto& line_posting : first_term_posting_entry->second) {
    const FileRecord* file_record = recordById(line_posting.documentId);
    if (file_record != nullptr && !line_posting.lines.empty()) {
      candidate_matches.push_back({*file_record, line_posting.lines});
    }
  }

  for (std::size_t term_index = 1; term_index < query_terms.size();
       ++term_index) {
    auto term_posting_entry = linePostings_.find(query_terms[term_index]);
    if (term_posting_entry == linePostings_.end()) {
      return {};
    }

    std::vector<LineMatch> next_candidate_matches;
    for (auto& candidate_match : candidate_matches) {
      const LinePosting* matching_posting = findLinePosting(
          term_posting_entry->second, candidate_match.record.id);
      if (matching_posting == nullptr) {
        continue;
      }

      std::vector<std::uint32_t> shared_lines;
      std::set_intersection(
          candidate_match.lines.begin(), candidate_match.lines.end(),
          matching_posting->lines.begin(), matching_posting->lines.end(),
          std::back_inserter(shared_lines));
      if (!shared_lines.empty()) {
        candidate_match.lines = std::move(shared_lines);
        next_candidate_matches.push_back(std::move(candidate_match));
      }
    }

    candidate_matches = std::move(next_candidate_matches);
    if (candidate_matches.empty()) {
      return {};
    }
  }

  std::vector<LineMatch> limited_matches;
  for (auto& candidate_match : candidate_matches) {
    limited_matches.push_back(std::move(candidate_match));
    if (limited_matches.size() >= limit) {
      break;
    }
  }
  return limited_matches;
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
  return static_cast<std::size_t>(
      std::count_if(records_.begin(), records_.end(),
                    [](const FileRecord& file_record) -> bool {
                      return file_record.textIndexed;
                    }));
}

auto InvertedIndex::termCount() const -> std::size_t {
  return linePostings_.size();
}

auto InvertedIndex::recordById(DocumentId document_id) const
    -> const FileRecord* {
  if (document_id >= records_.size()) {
    return nullptr;
  }
  return &records_[document_id];
}

auto InvertedIndex::lower(std::string input_text) -> std::string {
  std::transform(input_text.begin(), input_text.end(), input_text.begin(),
                 [](unsigned char character) -> char {
                   return static_cast<char>(std::tolower(character));
                 });
  return input_text;
}

auto InvertedIndex::containsCaseInsensitive(
    const std::string& text, const std::string& query_text) -> bool {
  return lower(text).find(lower(query_text)) != std::string::npos;
}

}  // namespace minisearch::index
