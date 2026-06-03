#include "minisearch/index/IndexStorage.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "index.pb.h"

namespace minisearch::index {

namespace {

constexpr std::uint32_t IndexFormatVersion = 1;

}  // namespace

auto IndexStorage::save(const std::filesystem::path &path,
                        const InvertedIndex &index,
                        const std::filesystem::path &rootPath) const -> void {
  proto::Index protoIndex;
  protoIndex.set_version(IndexFormatVersion);
  protoIndex.set_root_path(rootPath.string());

  for (const auto &record : index.records()) {
    // add_XXX returns a pointer to the newly added element, which we can fill
    // in directly
    proto::FileRecord *protoRecord = protoIndex.add_records();
    protoRecord->set_id(record.id);
    protoRecord->set_path(record.path.string());
    protoRecord->set_size(static_cast<std::uint64_t>(record.size));
    protoRecord->set_modified_time(
        static_cast<std::int64_t>(record.modifiedTime));
    protoRecord->set_text_indexed(record.textIndexed);
    protoRecord->set_content_hash(record.contentHash);
  }

  for (const auto &[term, postings] : index.postings()) {
    proto::PostingList *protoPosting = protoIndex.add_postings();
    protoPosting->set_term(term);

    for (const auto id : postings) {
      protoPosting->add_document_ids(id);
    }
  }

  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      throw std::runtime_error("failed to create index directory: " +
                               parent.string());
    }
  }

  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("failed to open index file for writing: " +
                             path.string());
  }

  if (!protoIndex.SerializeToOstream(&output)) {
    throw std::runtime_error("failed to serialize index file: " +
                             path.string());
  }
}

auto IndexStorage::load(const std::filesystem::path &path) const
    -> InvertedIndex {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open index file: " + path.string());
  }

  proto::Index protoIndex;
  if (!protoIndex.ParseFromIstream(&input)) {
    throw std::runtime_error("failed to parse index file: " + path.string());
  }

  if (protoIndex.version() != IndexFormatVersion) {
    throw std::runtime_error("unsupported index format: " + path.string());
  }

  std::vector<FileRecord> records;
  records.reserve(static_cast<std::size_t>(protoIndex.records_size()));
  for (const auto &protoRecord : protoIndex.records()) {
    FileRecord record;
    record.id = protoRecord.id();
    record.path = protoRecord.path();
    record.size = static_cast<std::uintmax_t>(protoRecord.size());
    record.modifiedTime = static_cast<std::time_t>(protoRecord.modified_time());
    record.textIndexed = protoRecord.text_indexed();
    record.contentHash = protoRecord.content_hash();
    records.push_back(std::move(record));
  }

  std::unordered_map<std::string, InvertedIndex::PostingList> postings;
  postings.reserve(static_cast<std::size_t>(protoIndex.postings_size()));
  for (const auto &protoPosting : protoIndex.postings()) {
    InvertedIndex::PostingList ids;
    ids.reserve(static_cast<std::size_t>(protoPosting.document_ids_size()));

    for (const auto id : protoPosting.document_ids()) {
      ids.push_back(id);
    }

    postings.emplace(protoPosting.term(), std::move(ids));
  }

  InvertedIndex index;
  index.replace(std::move(records), std::move(postings));
  return index;
}

}  // namespace minisearch::index
