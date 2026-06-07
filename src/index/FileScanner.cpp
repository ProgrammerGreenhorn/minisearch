#include "minisearch/index/FileScanner.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
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

auto isAllowedTextControl(unsigned char character) -> bool {
  return character == '\n' || character == '\r' || character == '\t' ||
         character == '\f';
}

auto isSuspiciousControl(unsigned char character) -> bool {
  if (character == '\0') {
    return true;
  }

  return (character < 0x20 && !isAllowedTextControl(character)) ||
         character == 0x7F;
}

auto controlRatioThreshold(double configured_threshold) -> double {
  if (configured_threshold < 0.0) {
    return 0.0;
  }
  if (configured_threshold > 1.0) {
    return 1.0;
  }
  return configured_threshold;
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

  if (file_size == 0 || options_.textProbeBytes == 0) {
    return true;
  }

  std::ifstream input_stream(file_path, std::ios::binary);
  if (!input_stream) {
    return false;
  }

  const std::size_t bytes_to_probe = static_cast<std::size_t>(std::min(
      file_size, static_cast<std::uintmax_t>(options_.textProbeBytes)));
  std::string probe_buffer(bytes_to_probe, '\0');
  input_stream.read(probe_buffer.data(),
                    static_cast<std::streamsize>(probe_buffer.size()));
  const std::size_t bytes_read =
      static_cast<std::size_t>(input_stream.gcount());
  if (bytes_read == 0) {
    return true;
  }

  std::size_t suspicious_control_count = 0;
  for (std::size_t byte_index = 0; byte_index < bytes_read; ++byte_index) {
    const unsigned char character =
        static_cast<unsigned char>(probe_buffer[byte_index]);
    if (character == '\0') {
      return false;
    }
    if (isSuspiciousControl(character)) {
      ++suspicious_control_count;
    }
  }

  const double suspicious_control_ratio =
      static_cast<double>(suspicious_control_count) /
      static_cast<double>(bytes_read);
  return suspicious_control_ratio <=
         controlRatioThreshold(options_.binaryControlRatio);
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

}  // namespace minisearch::index
