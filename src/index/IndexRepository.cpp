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
#include <vector>

#include "index.pb.h"
#include "minisearch/index/IndexCatalogStorage.hpp"
#include "minisearch/index/IndexSchema.hpp"
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
    throw std::runtime_error("no current index; run minisearch <path> first");
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
  if (catalogFileExists()) {
    IndexCatalogStorage index_catalog_storage;
    return index_catalog_storage.load(indexCatalogFile());
  }

  try {
    const CurrentIndex current_index = loadCurrentIndex();
    return {{current_index.rootPath, current_index.indexFile, 0}};
  } catch (const std::exception&) {
    return {};
  }
}

}  // namespace minisearch::index
