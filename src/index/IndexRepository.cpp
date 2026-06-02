#include "minisearch/index/IndexRepository.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include "index.pb.h"
#include "minisearch/util/Logger.hpp"

namespace minisearch::index {

// a priavte namespace for helper functions and constants used by
// IndexRepository, can not be accessed from other files in the index module
namespace {

constexpr std::string_view DataDirectoryName = ".minisearch";

constexpr std::string_view IndexesDirectoryName = "indexes";

constexpr std::string_view CurrentPointerFileName = "current.pb";

constexpr std::string_view IndexFileExtension = ".pb";

constexpr auto CurrentPointerFormatVersion = 1u;

auto logHomeOnce(const char* home) -> void {
  static std::once_flag flag;
  std::call_once(flag, [home]() -> void {
    MINISEARCH_LOG_INFO("HOME environment variable: " +
                        std::string(home ? home : "(null)"));
  });
}

}  // namespace

auto IndexRepository::dataRoot() -> std::filesystem::path {
  const auto* home = std::getenv("HOME");
  if (home == nullptr || std::string_view(home).empty()) {
    throw std::runtime_error("HOME is not set; cannot resolve ~/.minisearch");
  }

  logHomeOnce(home);
  return std::filesystem::path(home) / DataDirectoryName;
}

auto IndexRepository::indexesRoot() -> std::filesystem::path {
  return dataRoot() / IndexesDirectoryName;
}

auto IndexRepository::currentPointerFile() -> std::filesystem::path {
  return dataRoot() / CurrentPointerFileName;
}

auto IndexRepository::canonicalKey(const std::filesystem::path& indexedPath)
    -> std::string {
  namespace fs = std::filesystem;

  std::error_code error;
  auto absolutePath = fs::absolute(indexedPath, error);
  if (error) {
    absolutePath = indexedPath;
  }
  // weakly_canonical resolves symlinks and normalizes the path, but doesn't
  // fail if the path doesn't exist (which canonical does), so it's more
  // suitable for generating a stable key for the index file
  error.clear();
  auto canonicalPath = fs::weakly_canonical(absolutePath, error);
  if (error) {
    canonicalPath = absolutePath.lexically_normal();
  }

  return canonicalPath.string();
}

auto IndexRepository::indexFileForPath(const std::filesystem::path& indexedPath)
    -> std::filesystem::path {
  return indexesRoot() / (stableHash(canonicalKey(indexedPath)) +
                          std::string(IndexFileExtension));
}

auto IndexRepository::saveCurrentIndex(const std::string& rootPath,
                                       const std::filesystem::path& indexFile)
    -> void {
  std::error_code error;
  std::filesystem::create_directories(dataRoot(), error);
  if (error) {
    throw std::runtime_error("failed to create data directory: " +
                             dataRoot().string());
  }

  proto::CurrentIndex current;
  current.set_version(CurrentPointerFormatVersion);
  current.set_root_path(rootPath);
  current.set_index_file(canonicalKey(indexFile));

  std::ofstream output(currentPointerFile(), std::ios::binary);
  if (!output) {
    throw std::runtime_error("failed to write current index pointer: " +
                             currentPointerFile().string());
  }

  if (!current.SerializeToOstream(&output)) {
    throw std::runtime_error("failed to serialize current index pointer: " +
                             currentPointerFile().string());
  }
}

auto IndexRepository::loadCurrentIndex() -> CurrentIndex {
  std::ifstream input(currentPointerFile(), std::ios::binary);
  if (!input) {
    throw std::runtime_error(
        "no current index; run minisearch <path> first");
  }

  proto::CurrentIndex protoCurrent;
  if (!protoCurrent.ParseFromIstream(&input)) {
    throw std::runtime_error("current index pointer is corrupt: " +
                             currentPointerFile().string());
  }

  if (protoCurrent.version() != CurrentPointerFormatVersion) {
    throw std::runtime_error("unsupported current index pointer format: " +
                             currentPointerFile().string());
  }

  CurrentIndex current;
  current.rootPath = protoCurrent.root_path();
  current.indexFile = protoCurrent.index_file();
  if (current.rootPath.empty() || current.indexFile.empty()) {
    throw std::runtime_error(
        "current index pointer is missing required data: " +
        currentPointerFile().string());
  }

  return current;
}

auto IndexRepository::stableHash(std::string_view value) -> std::string {
  constexpr auto offsetBasis = 14695981039346656037ull;
  constexpr auto prime = 1099511628211ull;

  auto hash = offsetBasis;
  for (const auto ch : value) {
    hash ^= static_cast<unsigned char>(ch);
    hash *= prime;
  }

  std::ostringstream stream;
  stream << std::hex << std::setw(16) << std::setfill('0') << hash;
  return stream.str();
}

}  // namespace minisearch::index
