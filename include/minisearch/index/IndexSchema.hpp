#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace minisearch::index {

struct SchemaVersionRange {
  std::uint32_t minimumSupportedVersion = 0;
  std::uint32_t currentVersion = 0;
};

/**
 * @brief Centralize protobuf schema versions and compatibility checks.
 *
 * IndexSchema defines the supported version ranges for persisted index files,
 * current-index pointers, and recent-index catalogs.
 */
class IndexSchema {
 public:
  /**
   * @brief Get the current persisted index schema version.
   *
   * @return Current index schema version written by MiniSearch.
   */
  static auto currentIndexVersion() -> std::uint32_t;

  /**
   * @brief Get the oldest index schema version that can still be loaded.
   *
   * @return Minimum supported index schema version.
   */
  static auto minimumSupportedIndexVersion() -> std::uint32_t;

  /**
   * @brief Get the supported index schema version range.
   *
   * @return Supported index schema range.
   */
  static auto indexVersionRange() -> SchemaVersionRange;

  /**
   * @brief Get the current-index pointer schema version.
   *
   * @return Current-index pointer schema version written by MiniSearch.
   */
  static auto currentIndexPointerVersion() -> std::uint32_t;

  /**
   * @brief Get the oldest current-index pointer schema version that can load.
   *
   * @return Minimum supported current-index pointer schema version.
   */
  static auto minimumSupportedIndexPointerVersion() -> std::uint32_t;

  /**
   * @brief Get the supported current-index pointer schema version range.
   *
   * @return Supported current-index pointer schema range.
   */
  static auto indexPointerVersionRange() -> SchemaVersionRange;

  /**
   * @brief Get the current index-catalog schema version.
   *
   * @return Current index-catalog schema version written by MiniSearch.
   */
  static auto currentIndexCatalogVersion() -> std::uint32_t;

  /**
   * @brief Get the oldest index-catalog schema version that can load.
   *
   * @return Minimum supported index-catalog schema version.
   */
  static auto minimumSupportedIndexCatalogVersion() -> std::uint32_t;

  /**
   * @brief Get the supported index-catalog schema version range.
   *
   * @return Supported index-catalog schema range.
   */
  static auto indexCatalogVersionRange() -> SchemaVersionRange;

  /**
   * @brief Check whether an index schema version can be loaded.
   *
   * @param schema_version Index schema version from persisted data.
   * @return True when the version is supported.
   */
  static auto isIndexVersionSupported(std::uint32_t schema_version) -> bool;

  /**
   * @brief Check whether a current-index pointer version can be loaded.
   *
   * @param schema_version Current-index pointer schema version from persisted
   * data.
   * @return True when the version is supported.
   */
  static auto isIndexPointerVersionSupported(std::uint32_t schema_version)
      -> bool;

  /**
   * @brief Check whether an index-catalog version can be loaded.
   *
   * @param schema_version Index-catalog schema version from persisted data.
   * @return True when the version is supported.
   */
  static auto isIndexCatalogVersionSupported(std::uint32_t schema_version)
      -> bool;

  /**
   * @brief Format an unsupported schema-version message.
   *
   * @param schema_name Human-readable schema name.
   * @param schema_version Persisted schema version that failed validation.
   * @param version_range Supported schema version range.
   * @return Human-readable error message.
   */
  static auto unsupportedVersionMessage(
      std::string_view schema_name, std::uint32_t schema_version,
      SchemaVersionRange version_range) -> std::string;
};

}  // namespace minisearch::index
