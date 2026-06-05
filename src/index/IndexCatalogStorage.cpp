#include "minisearch/index/IndexCatalogStorage.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "index.pb.h"
#include "minisearch/index/IndexSchema.hpp"

namespace minisearch::index {

namespace {

auto unsupportedIndexCatalogVersionMessage(
    std::uint32_t schema_version,
    const std::filesystem::path& catalog_file) -> std::string {
  return IndexSchema::unsupportedVersionMessage(
             "index catalog", schema_version,
             IndexSchema::indexCatalogVersionRange()) +
         ": " + catalog_file.string();
}

auto missingIndexCatalogMigrationMessage(
    std::uint32_t schema_version,
    const std::filesystem::path& catalog_file) -> std::string {
  return "missing index catalog migration helper for version " +
         std::to_string(schema_version) + ": " + catalog_file.string();
}

auto migrateIndexCatalogToCurrentVersion(
    proto::IndexCatalog& proto_catalog,
    const std::filesystem::path& catalog_file) -> void {
  if (!IndexSchema::isIndexCatalogVersionSupported(proto_catalog.version())) {
    throw std::runtime_error(unsupportedIndexCatalogVersionMessage(
        proto_catalog.version(), catalog_file));
  }

  while (proto_catalog.version() < IndexSchema::currentIndexCatalogVersion()) {
    switch (proto_catalog.version()) {
      default:
        throw std::runtime_error(missingIndexCatalogMigrationMessage(
            proto_catalog.version(), catalog_file));
    }
  }
}

}  // namespace

auto IndexCatalogStorage::load(const std::filesystem::path& catalog_file) const
    -> std::vector<ManagedIndex> {
  std::ifstream input_stream(catalog_file, std::ios::binary);
  if (!input_stream) {
    return {};
  }

  proto::IndexCatalog proto_catalog;
  if (!proto_catalog.ParseFromIstream(&input_stream)) {
    throw std::runtime_error("index catalog is corrupt: " +
                             catalog_file.string());
  }

  migrateIndexCatalogToCurrentVersion(proto_catalog, catalog_file);

  std::vector<ManagedIndex> managed_indexes;
  managed_indexes.reserve(
      static_cast<std::size_t>(proto_catalog.entries_size()));
  for (const auto& proto_entry : proto_catalog.entries()) {
    if (proto_entry.root_path().empty() || proto_entry.index_file().empty()) {
      continue;
    }

    ManagedIndex managed_index;
    managed_index.rootPath = proto_entry.root_path();
    managed_index.indexFile = proto_entry.index_file();
    managed_index.updatedTime =
        static_cast<std::time_t>(proto_entry.updated_time());
    managed_indexes.push_back(std::move(managed_index));
  }
  return managed_indexes;
}

auto IndexCatalogStorage::save(
    const std::filesystem::path& catalog_file,
    const std::vector<ManagedIndex>& managed_indexes) const -> void {
  const std::filesystem::path parent_directory = catalog_file.parent_path();
  if (!parent_directory.empty()) {
    std::error_code create_error;
    std::filesystem::create_directories(parent_directory, create_error);
    if (create_error) {
      throw std::runtime_error("failed to create index catalog directory: " +
                               parent_directory.string());
    }
  }

  proto::IndexCatalog proto_catalog;
  proto_catalog.set_version(IndexSchema::currentIndexCatalogVersion());
  for (const auto& managed_index : managed_indexes) {
    if (managed_index.rootPath.empty() || managed_index.indexFile.empty()) {
      continue;
    }

    proto::IndexCatalogEntry* proto_entry = proto_catalog.add_entries();
    proto_entry->set_root_path(managed_index.rootPath);
    proto_entry->set_index_file(managed_index.indexFile.string());
    proto_entry->set_updated_time(
        static_cast<std::int64_t>(managed_index.updatedTime));
  }

  std::ofstream output_stream(catalog_file, std::ios::binary);
  if (!output_stream) {
    throw std::runtime_error("failed to write index catalog: " +
                             catalog_file.string());
  }

  if (!proto_catalog.SerializeToOstream(&output_stream)) {
    throw std::runtime_error("failed to serialize index catalog: " +
                             catalog_file.string());
  }
}

}  // namespace minisearch::index
