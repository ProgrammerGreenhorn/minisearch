#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <utility>

namespace minisearch::index {

struct FileRecord {
  using Id = std::uint32_t;

  /**
   * @brief Create an empty file record.
   */
  FileRecord() = default;

  /**
   * @brief Create a file record before a document id is assigned.
   *
   * @param file_path File path represented by this record.
   * @param file_size File size in bytes.
   * @param modified_time Last modification time as a time_t value.
   * @param text_indexed Whether the file content is indexed as text.
   * @param content_hash Stable content hash for indexed text files.
   */
  FileRecord(std::filesystem::path file_path, std::uintmax_t file_size,
             std::time_t modified_time, bool text_indexed,
             std::uint64_t content_hash)
      : path(std::move(file_path)),
        size(file_size),
        modifiedTime(modified_time),
        textIndexed(text_indexed),
        contentHash(content_hash) {}

  /**
   * @brief Create a file record with an existing document id.
   *
   * @param document_id Document id assigned to this file.
   * @param file_path File path represented by this record.
   * @param file_size File size in bytes.
   * @param modified_time Last modification time as a time_t value.
   * @param text_indexed Whether the file content is indexed as text.
   * @param content_hash Stable content hash for indexed text files.
   */
  FileRecord(Id document_id, std::filesystem::path file_path,
             std::uintmax_t file_size, std::time_t modified_time,
             bool text_indexed, std::uint64_t content_hash)
      : id(document_id),
        path(std::move(file_path)),
        size(file_size),
        modifiedTime(modified_time),
        textIndexed(text_indexed),
        contentHash(content_hash) {}

  Id id = 0;
  std::filesystem::path path;
  std::uintmax_t size = 0;
  std::time_t modifiedTime = 0;
  bool textIndexed = false;
  std::uint64_t contentHash = 0;
};

}  // namespace minisearch::index
