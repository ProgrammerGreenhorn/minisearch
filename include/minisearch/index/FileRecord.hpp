#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <utility>

namespace minisearch::index {

struct FileRecord {
    using Id = std::uint32_t;

    FileRecord() = default;

    FileRecord(std::filesystem::path path,
               std::uintmax_t size,
               std::time_t modifiedTime,
               bool textIndexed)
        : path(std::move(path)),
          size(size),
          modifiedTime(modifiedTime),
          textIndexed(textIndexed)
    {
    }

    FileRecord(Id id,
               std::filesystem::path path,
               std::uintmax_t size,
               std::time_t modifiedTime,
               bool textIndexed)
        : id(id),
          path(std::move(path)),
          size(size),
          modifiedTime(modifiedTime),
          textIndexed(textIndexed)
    {
    }

    Id id = 0;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::time_t modifiedTime = 0;
    bool textIndexed = false;
};

} // namespace minisearch::index
