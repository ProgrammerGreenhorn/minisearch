#include "minisearch/index/IndexStorage.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "index.pb.h"
#include "minisearch/index/IndexSchema.hpp"

namespace minisearch::index {

namespace {

auto unsupportedIndexVersionMessage(std::uint32_t schema_version,
                                    const std::filesystem::path &index_file)
    -> std::string {
  return IndexSchema::unsupportedVersionMessage(
             "index", schema_version, IndexSchema::indexVersionRange()) +
         ": " + index_file.string();
}

auto missingIndexMigrationMessage(std::uint32_t schema_version,
                                  const std::filesystem::path &index_file)
    -> std::string {
  return "missing index migration helper for version " +
         std::to_string(schema_version) + ": " + index_file.string();
}

auto migrateIndexV1ToV2(proto::Index &proto_index) -> void {
  // Version 2 added root_path metadata. The searchable records and postings
  // are unchanged, so older indexes can be loaded by marking them current.
  proto_index.set_version(IndexSchema::currentIndexVersion());
}

auto migrateIndexToCurrentVersion(proto::Index &proto_index,
                                  const std::filesystem::path &index_file)
    -> void {
  if (!IndexSchema::isIndexVersionSupported(proto_index.version())) {
    throw std::runtime_error(
        unsupportedIndexVersionMessage(proto_index.version(), index_file));
  }

  while (proto_index.version() < IndexSchema::currentIndexVersion()) {
    switch (proto_index.version()) {
      case 1:
        migrateIndexV1ToV2(proto_index);
        break;
      default:
        throw std::runtime_error(
            missingIndexMigrationMessage(proto_index.version(), index_file));
    }
  }
}

auto loadProtoIndex(const std::filesystem::path &index_file) -> proto::Index {
  std::ifstream input_stream(index_file, std::ios::binary);
  if (!input_stream) {
    throw std::runtime_error("failed to open index file: " +
                             index_file.string());
  }

  proto::Index proto_index;
  if (!proto_index.ParseFromIstream(&input_stream)) {
    throw std::runtime_error("failed to parse index file: " +
                             index_file.string());
  }

  migrateIndexToCurrentVersion(proto_index, index_file);
  return proto_index;
}

}  // namespace

auto IndexStorage::save(const std::filesystem::path &index_file,
                        const InvertedIndex &search_index,
                        const std::filesystem::path &root_path) const -> void {
  proto::Index proto_index;
  proto_index.set_version(IndexSchema::currentIndexVersion());
  proto_index.set_root_path(root_path.string());

  for (const auto &file_record : search_index.records()) {
    // add_XXX returns a pointer to the newly added element, which we can fill
    // in directly
    proto::FileRecord *proto_record = proto_index.add_records();
    proto_record->set_id(file_record.id);
    proto_record->set_path(file_record.path.string());
    proto_record->set_size(static_cast<std::uint64_t>(file_record.size));
    proto_record->set_modified_time(
        static_cast<std::int64_t>(file_record.modifiedTime));
    proto_record->set_text_indexed(file_record.textIndexed);
    proto_record->set_content_hash(file_record.contentHash);
  }

  for (const auto &[term_text, line_postings] : search_index.linePostings()) {
    proto::PostingList *proto_posting_list = proto_index.add_postings();
    proto_posting_list->set_term(term_text);

    for (const auto &line_posting : line_postings) {
      proto::Posting *proto_line_posting = proto_posting_list->add_postings();
      proto_line_posting->set_document_id(line_posting.documentId);
      for (const auto line_number : line_posting.lines) {
        proto_line_posting->add_lines(line_number);
      }
    }
  }

  const std::filesystem::path parent_directory = index_file.parent_path();
  if (!parent_directory.empty()) {
    std::error_code create_error;
    std::filesystem::create_directories(parent_directory, create_error);
    if (create_error) {
      throw std::runtime_error("failed to create index directory: " +
                               parent_directory.string());
    }
  }

  std::ofstream output_stream(index_file, std::ios::binary);
  if (!output_stream) {
    throw std::runtime_error("failed to open index file for writing: " +
                             index_file.string());
  }

  if (!proto_index.SerializeToOstream(&output_stream)) {
    throw std::runtime_error("failed to serialize index file: " +
                             index_file.string());
  }
}

auto IndexStorage::load(const std::filesystem::path &index_file) const
    -> InvertedIndex {
  const proto::Index proto_index = loadProtoIndex(index_file);

  std::vector<FileRecord> file_records;
  file_records.reserve(static_cast<std::size_t>(proto_index.records_size()));
  for (const auto &proto_record : proto_index.records()) {
    FileRecord file_record;
    file_record.id = proto_record.id();
    file_record.path = proto_record.path();
    file_record.size = static_cast<std::uintmax_t>(proto_record.size());
    file_record.modifiedTime =
        static_cast<std::time_t>(proto_record.modified_time());
    file_record.textIndexed = proto_record.text_indexed();
    file_record.contentHash = proto_record.content_hash();
    file_records.push_back(std::move(file_record));
  }

  std::unordered_map<std::string, InvertedIndex::LinePostingList> line_postings;
  line_postings.reserve(static_cast<std::size_t>(proto_index.postings_size()));
  for (const auto &proto_posting_list : proto_index.postings()) {
    InvertedIndex::LinePostingList posting_list;
    posting_list.reserve(
        static_cast<std::size_t>(proto_posting_list.postings_size()));

    for (const auto &proto_line_posting : proto_posting_list.postings()) {
      InvertedIndex::LinePosting line_posting;
      line_posting.documentId = proto_line_posting.document_id();
      line_posting.lines.reserve(
          static_cast<std::size_t>(proto_line_posting.lines_size()));
      for (const auto line_number : proto_line_posting.lines()) {
        line_posting.lines.push_back(line_number);
      }
      posting_list.push_back(std::move(line_posting));
    }

    line_postings.emplace(proto_posting_list.term(), std::move(posting_list));
  }

  InvertedIndex loaded_index;
  loaded_index.replace(std::move(file_records), std::move(line_postings));
  return loaded_index;
}

auto IndexStorage::loadMetadata(const std::filesystem::path &index_file) const
    -> Metadata {
  const proto::Index proto_index = loadProtoIndex(index_file);

  Metadata metadata;
  metadata.rootPath = proto_index.root_path();
  return metadata;
}

}  // namespace minisearch::index
