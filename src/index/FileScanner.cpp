#include "minisearch/index/FileScanner.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

#include "minisearch/util/Hash.hpp"
#include "minisearch/util/Logger.hpp"

namespace minisearch::index {

namespace {

auto normalizePath(const std::filesystem::path& source_path)
    -> std::filesystem::path {
  return std::filesystem::absolute(source_path).lexically_normal();
}

}  // namespace

FileScanner::FileScanner() : FileScanner(Options{}) {}

FileScanner::FileScanner(Options scanner_options)
    : options_(std::move(scanner_options)) {}

auto FileScanner::scan(const std::filesystem::path& root_path) const
    -> std::vector<FileRecord> {
  namespace fs = std::filesystem;

  if (!fs::exists(root_path)) {
    throw std::runtime_error("path does not exist: " + root_path.string());
  }

  std::vector<FileRecord> file_records;
  const fs::path normalized_root = normalizePath(root_path);
  MINISEARCH_LOG_DEBUG("normalized path: " + normalized_root.string());

  if (fs::is_regular_file(normalized_root)) {
    const std::uintmax_t file_size = fs::file_size(normalized_root);
    const bool text_indexed = shouldIndexText(normalized_root, file_size);
    file_records.emplace_back(
        normalized_root, file_size,
        toTimeT(fs::last_write_time(normalized_root)), text_indexed,
        text_indexed ? util::stableFileHash(normalized_root) : 0);
    return file_records;
  }

  const fs::directory_options iterator_options =
      fs::directory_options::skip_permission_denied;
  auto directory_iterator =
      fs::recursive_directory_iterator(normalized_root, iterator_options);
  auto directory_end = fs::recursive_directory_iterator{};
  for (; directory_iterator != directory_end; ++directory_iterator) {
    const auto& current_entry = *directory_iterator;
    if (shouldSkip(current_entry)) {
      if (current_entry.is_directory()) {
        directory_iterator.disable_recursion_pending();
      }
      continue;
    }

    if (!current_entry.is_regular_file()) {
      continue;
    }

    std::error_code file_size_error;
    const std::uintmax_t file_size = current_entry.file_size(file_size_error);
    if (file_size_error) {
      continue;
    }

    const bool text_indexed = shouldIndexText(current_entry.path(), file_size);
    file_records.emplace_back(
        current_entry.path(), file_size,
        toTimeT(current_entry.last_write_time()), text_indexed,
        text_indexed ? util::stableFileHash(current_entry.path()) : 0);
  }

  return file_records;
}

auto FileScanner::shouldSkip(
    const std::filesystem::directory_entry& directory_entry) const -> bool {
  const std::string entry_name = directory_entry.path().filename().string();
  return std::find(options_.excludedNames.begin(), options_.excludedNames.end(),
                   entry_name) != options_.excludedNames.end();
}

auto FileScanner::shouldIndexText(const std::filesystem::path& file_path,
                                  std::uintmax_t file_size) const -> bool {
  if (file_size > options_.maxTextFileBytes) {
    return false;
  }

  const std::string file_extension = lower(file_path.extension().string());
  return std::find(options_.textExtensions.begin(),
                   options_.textExtensions.end(),
                   file_extension) != options_.textExtensions.end();
}

auto FileScanner::toTimeT(std::filesystem::file_time_type file_time)
    -> std::time_t {
  using namespace std::chrono;
  const system_clock::time_point system_time =
      time_point_cast<system_clock::duration>(
          file_time - std::filesystem::file_time_type::clock::now() +
          system_clock::now());
  return system_clock::to_time_t(system_time);
}

auto FileScanner::lower(std::string input_text) -> std::string {
  std::transform(input_text.begin(), input_text.end(), input_text.begin(),
                 [](unsigned char character) -> char {
                   return static_cast<char>(std::tolower(character));
                 });
  return input_text;
}

}  // namespace minisearch::index
