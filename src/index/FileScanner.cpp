#include "minisearch/index/FileScanner.hpp"
#include "minisearch/util/Hash.hpp"
#include "minisearch/util/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace minisearch::index {

namespace {

auto normalizePath(const std::filesystem::path& path) -> std::filesystem::path {
  return std::filesystem::absolute(path).lexically_normal();
}

}  // namespace

FileScanner::FileScanner() : FileScanner(Options{}) {}

FileScanner::FileScanner(Options options) : options_(std::move(options)) {}

auto FileScanner::scan(const std::filesystem::path& root) const
    -> std::vector<FileRecord> {
  namespace fs = std::filesystem;

  if (!fs::exists(root)) {
    throw std::runtime_error("path does not exist: " + root.string());
  }

  std::vector<FileRecord> records;
  const fs::path base = normalizePath(root);
  MINISEARCH_LOG_DEBUG("normalized path: " + base.string());

  if (fs::is_regular_file(base)) {
    const std::uintmax_t size = fs::file_size(base);
    const bool textIndexed = shouldIndexText(base, size);
    records.emplace_back(base, size, toTimeT(fs::last_write_time(base)),
                         textIndexed,
                         textIndexed ? util::stableFileHash(base) : 0);
    return records;
  }

  const fs::directory_options iteratorOptions =
      fs::directory_options::skip_permission_denied;
  auto it = fs::recursive_directory_iterator(base, iteratorOptions);
  auto end = fs::recursive_directory_iterator{};
  for (; it != end; ++it) {
    const auto& entry = *it;
    if (shouldSkip(entry)) {
      if (entry.is_directory()) {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!entry.is_regular_file()) {
      continue;
    }

    std::error_code error;
    const std::uintmax_t size = entry.file_size(error);
    if (error) {
      continue;
    }

    const bool textIndexed = shouldIndexText(entry.path(), size);
    records.emplace_back(entry.path(), size, toTimeT(entry.last_write_time()),
                         textIndexed,
                         textIndexed ? util::stableFileHash(entry.path()) : 0);
  }

  return records;
}

auto FileScanner::shouldSkip(
    const std::filesystem::directory_entry& entry) const -> bool {
  const std::string name = entry.path().filename().string();
  return std::find(options_.excludedNames.begin(), options_.excludedNames.end(),
                   name) != options_.excludedNames.end();
}

auto FileScanner::shouldIndexText(const std::filesystem::path& path,
                                  std::uintmax_t size) const -> bool {
  if (size > options_.maxTextFileBytes) {
    return false;
  }

  const std::string extension = lower(path.extension().string());
  return std::find(options_.textExtensions.begin(),
                   options_.textExtensions.end(),
                   extension) != options_.textExtensions.end();
}

auto FileScanner::toTimeT(std::filesystem::file_time_type value)
    -> std::time_t {
  using namespace std::chrono;
  const system_clock::time_point systemTime =
      time_point_cast<system_clock::duration>(
      value - std::filesystem::file_time_type::clock::now() +
      system_clock::now());
  return system_clock::to_time_t(systemTime);
}

auto FileScanner::lower(std::string value) -> std::string {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) -> char {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

}  // namespace minisearch::index
