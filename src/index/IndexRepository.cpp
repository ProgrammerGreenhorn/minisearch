#include "minisearch/index/IndexRepository.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "index.pb.h"
#include "minisearch/index/IndexCatalogStorage.hpp"
#include "minisearch/index/IndexSchema.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/util/Hash.hpp"
#include "minisearch/util/Logger.hpp"

namespace minisearch::index {

// a priavte namespace for helper functions and constants used by
// IndexRepository, can not be accessed from other files in the index module
namespace {

constexpr std::string_view DataDirectoryName = ".minisearch";

constexpr std::string_view IndexesDirectoryName = "indexes";

constexpr std::string_view CurrentPointerFileName = "current.pb";

constexpr std::string_view IndexCatalogFileName = "indexes.pb";

constexpr std::string_view IndexFileExtension = ".pb";

constexpr std::size_t MaxRecentIndexes = 20;

auto unsupportedIndexPointerVersionMessage(std::uint32_t schema_version)
    -> std::string {
  return IndexSchema::unsupportedVersionMessage(
             "current index pointer", schema_version,
             IndexSchema::indexPointerVersionRange()) +
         ": " + IndexRepository::currentPointerFile().string();
}

auto missingIndexPointerMigrationMessage(std::uint32_t schema_version)
    -> std::string {
  return "missing current index pointer migration helper for version " +
         std::to_string(schema_version) + ": " +
         IndexRepository::currentPointerFile().string();
}

auto migrateCurrentIndexToCurrentVersion(
    proto::CurrentIndex& proto_current_index) -> void {
  if (!IndexSchema::isIndexPointerVersionSupported(
          proto_current_index.version())) {
    throw std::runtime_error(
        unsupportedIndexPointerVersionMessage(proto_current_index.version()));
  }

  while (proto_current_index.version() <
         IndexSchema::currentIndexPointerVersion()) {
    switch (proto_current_index.version()) {
      default:
        throw std::runtime_error(
            missingIndexPointerMigrationMessage(proto_current_index.version()));
    }
  }
}

auto catalogFileExists() -> bool {
  std::error_code exists_error;
  const bool catalog_exists = std::filesystem::exists(
      IndexRepository::indexCatalogFile(), exists_error);
  if (exists_error) {
    throw std::runtime_error("failed to check index catalog: " +
                             IndexRepository::indexCatalogFile().string());
  }
  return catalog_exists;
}

auto appendBackfilledIndexes(
    std::vector<IndexRepository::ManagedIndex>& managed_indexes) -> bool {
  std::error_code directory_error;
  const bool indexes_directory_exists =
      std::filesystem::exists(IndexRepository::indexesRoot(), directory_error);
  if (directory_error || !indexes_directory_exists) {
    return false;
  }

  std::unordered_set<std::string> known_roots;
  std::unordered_set<std::string> known_index_files;
  for (const auto& managed_index : managed_indexes) {
    known_roots.insert(managed_index.rootPath);
    known_index_files.insert(managed_index.indexFile.string());
  }

  bool changed = false;
  IndexStorage index_storage;
  std::filesystem::directory_iterator directory_iterator(
      IndexRepository::indexesRoot(), directory_error);
  if (directory_error) {
    return false;
  }

  for (const auto& directory_entry : directory_iterator) {
    std::error_code entry_error;
    if (!directory_entry.is_regular_file(entry_error) || entry_error) {
      continue;
    }

    const std::filesystem::path index_file = directory_entry.path();
    if (index_file.extension() != IndexFileExtension) {
      continue;
    }

    const std::string index_file_key = index_file.string();
    if (known_index_files.find(index_file_key) != known_index_files.end()) {
      continue;
    }

    try {
      const IndexStorage::Metadata metadata =
          index_storage.loadMetadata(index_file);
      if (metadata.rootPath.empty() ||
          known_roots.find(metadata.rootPath) != known_roots.end()) {
        continue;
      }

      managed_indexes.push_back({metadata.rootPath, index_file, 0});
      known_roots.insert(metadata.rootPath);
      known_index_files.insert(index_file_key);
      changed = true;
    } catch (const std::exception& exception) {
      MINISEARCH_LOG_WARNING(
          "skipping unreadable index catalog backfill file: " +
          index_file.string() + ": " + exception.what());
    }
  }

  if (managed_indexes.size() > MaxRecentIndexes) {
    managed_indexes.resize(MaxRecentIndexes);
  }
  return changed;
}

// saves the given index as the current index and adds it to the recent indexes
// catalog, removing any existing entries with the same root path or index file
auto saveManagedIndex(const std::string& root_path,
                      const std::filesystem::path& index_file) -> void {
  IndexCatalogStorage index_catalog_storage;
  std::vector<IndexRepository::ManagedIndex> managed_indexes =
      index_catalog_storage.load(IndexRepository::indexCatalogFile());

  const std::filesystem::path canonical_index_file =
      IndexRepository::canonicalKey(index_file);
  managed_indexes.erase(
      std::remove_if(managed_indexes.begin(), managed_indexes.end(),
                     [&root_path, &canonical_index_file](
                         const IndexRepository::ManagedIndex& managed_index) {
                       return managed_index.rootPath == root_path ||
                              managed_index.indexFile == canonical_index_file;
                     }),
      managed_indexes.end());

  managed_indexes.insert(managed_indexes.begin(),
                         {root_path, canonical_index_file, std::time(nullptr)});
  if (managed_indexes.size() > MaxRecentIndexes) {
    managed_indexes.resize(MaxRecentIndexes);
  }

  index_catalog_storage.save(IndexRepository::indexCatalogFile(),
                             managed_indexes);
}

auto logHomeOnce(const char* home_value) -> void {
  static std::once_flag log_once_flag;
  std::call_once(log_once_flag, [home_value]() -> void {
    MINISEARCH_LOG_INFO("HOME environment variable: " +
                        std::string(home_value ? home_value : "(null)"));
  });
}

}  // namespace

auto IndexRepository::dataRoot() -> std::filesystem::path {
  const char* home_value = std::getenv("HOME");
  if (home_value == nullptr || std::string_view(home_value).empty()) {
    throw std::runtime_error("HOME is not set; cannot resolve ~/.minisearch");
  }

  logHomeOnce(home_value);
  return std::filesystem::path(home_value) / DataDirectoryName;
}

auto IndexRepository::indexesRoot() -> std::filesystem::path {
  return dataRoot() / IndexesDirectoryName;
}

auto IndexRepository::currentPointerFile() -> std::filesystem::path {
  return dataRoot() / CurrentPointerFileName;
}

auto IndexRepository::indexCatalogFile() -> std::filesystem::path {
  return dataRoot() / IndexCatalogFileName;
}

auto IndexRepository::canonicalKey(const std::filesystem::path& indexed_path)
    -> std::string {
  namespace fs = std::filesystem;

  std::error_code path_error;
  fs::path absolute_path = fs::absolute(indexed_path, path_error);
  if (path_error) {
    absolute_path = indexed_path;
  }
  // weakly_canonical resolves symlinks and normalizes the path, but doesn't
  // fail if the path doesn't exist (which canonical does), so it's more
  // suitable for generating a stable key for the index file
  path_error.clear();
  fs::path canonical_path = fs::weakly_canonical(absolute_path, path_error);
  if (path_error) {
    canonical_path = absolute_path.lexically_normal();
  }

  return canonical_path.string();
}

auto IndexRepository::indexFileForPath(
    const std::filesystem::path& indexed_path) -> std::filesystem::path {
  return indexesRoot() /
         (util::hashToHex(util::stableHash(canonicalKey(indexed_path))) +
          std::string(IndexFileExtension));
}

auto IndexRepository::saveCurrentIndex(const std::string& root_path,
                                       const std::filesystem::path& index_file)
    -> void {
  std::error_code create_error;
  std::filesystem::create_directories(dataRoot(), create_error);
  if (create_error) {
    throw std::runtime_error("failed to create data directory: " +
                             dataRoot().string());
  }

  proto::CurrentIndex current_index;
  current_index.set_version(IndexSchema::currentIndexPointerVersion());
  current_index.set_root_path(root_path);
  current_index.set_index_file(canonicalKey(index_file));

  std::ofstream output_stream(currentPointerFile(), std::ios::binary);
  if (!output_stream) {
    throw std::runtime_error("failed to write current index pointer: " +
                             IndexRepository::currentPointerFile().string());
  }

  if (!current_index.SerializeToOstream(&output_stream)) {
    throw std::runtime_error("failed to serialize current index pointer: " +
                             IndexRepository::currentPointerFile().string());
  }

  saveManagedIndex(root_path, index_file);
}

auto IndexRepository::loadCurrentIndex() -> CurrentIndex {
  std::ifstream input_stream(currentPointerFile(), std::ios::binary);
  if (!input_stream) {
    throw std::runtime_error("no current index; use index <path> in the shell");
  }

  proto::CurrentIndex proto_current_index;
  if (!proto_current_index.ParseFromIstream(&input_stream)) {
    throw std::runtime_error("current index pointer is corrupt: " +
                             IndexRepository::currentPointerFile().string());
  }

  migrateCurrentIndexToCurrentVersion(proto_current_index);

  CurrentIndex current_index;
  current_index.rootPath = proto_current_index.root_path();
  current_index.indexFile = proto_current_index.index_file();
  if (current_index.rootPath.empty() || current_index.indexFile.empty()) {
    throw std::runtime_error(
        "current index pointer is missing required data: " +
        IndexRepository::currentPointerFile().string());
  }

  return current_index;
}

auto IndexRepository::loadRecentIndexes() -> std::vector<ManagedIndex> {
  IndexCatalogStorage index_catalog_storage;
  std::vector<ManagedIndex> managed_indexes;
  if (catalogFileExists()) {
    managed_indexes = index_catalog_storage.load(indexCatalogFile());
  } else {
    try {
      const CurrentIndex current_index = loadCurrentIndex();
      managed_indexes.push_back(
          {current_index.rootPath, current_index.indexFile, 0});
    } catch (const std::exception&) {
    }
  }

  if (appendBackfilledIndexes(managed_indexes)) {
    index_catalog_storage.save(indexCatalogFile(), managed_indexes);
  }

  return managed_indexes;
}

}  // namespace minisearch::index
