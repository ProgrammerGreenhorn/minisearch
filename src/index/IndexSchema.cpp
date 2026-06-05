#include "minisearch/index/IndexSchema.hpp"

#include <sstream>

namespace minisearch::index {

namespace {

constexpr std::uint32_t CurrentIndexVersion = 2;
constexpr std::uint32_t MinimumSupportedIndexVersion = 1;
constexpr std::uint32_t CurrentIndexPointerVersion = 1;
constexpr std::uint32_t MinimumSupportedIndexPointerVersion = 1;
constexpr std::uint32_t CurrentIndexCatalogVersion = 1;
constexpr std::uint32_t MinimumSupportedIndexCatalogVersion = 1;

auto isVersionInRange(std::uint32_t schema_version,
                      SchemaVersionRange version_range) -> bool {
  return schema_version >= version_range.minimumSupportedVersion &&
         schema_version <= version_range.currentVersion;
}

}  // namespace

auto IndexSchema::currentIndexVersion() -> std::uint32_t {
  return CurrentIndexVersion;
}

auto IndexSchema::minimumSupportedIndexVersion() -> std::uint32_t {
  return MinimumSupportedIndexVersion;
}

auto IndexSchema::indexVersionRange() -> SchemaVersionRange {
  return {MinimumSupportedIndexVersion, CurrentIndexVersion};
}

auto IndexSchema::currentIndexPointerVersion() -> std::uint32_t {
  return CurrentIndexPointerVersion;
}

auto IndexSchema::minimumSupportedIndexPointerVersion() -> std::uint32_t {
  return MinimumSupportedIndexPointerVersion;
}

auto IndexSchema::indexPointerVersionRange() -> SchemaVersionRange {
  return {MinimumSupportedIndexPointerVersion, CurrentIndexPointerVersion};
}

auto IndexSchema::currentIndexCatalogVersion() -> std::uint32_t {
  return CurrentIndexCatalogVersion;
}

auto IndexSchema::minimumSupportedIndexCatalogVersion() -> std::uint32_t {
  return MinimumSupportedIndexCatalogVersion;
}

auto IndexSchema::indexCatalogVersionRange() -> SchemaVersionRange {
  return {MinimumSupportedIndexCatalogVersion, CurrentIndexCatalogVersion};
}

auto IndexSchema::isIndexVersionSupported(std::uint32_t schema_version)
    -> bool {
  return isVersionInRange(schema_version, indexVersionRange());
}

auto IndexSchema::isIndexPointerVersionSupported(std::uint32_t schema_version)
    -> bool {
  return isVersionInRange(schema_version, indexPointerVersionRange());
}

auto IndexSchema::isIndexCatalogVersionSupported(std::uint32_t schema_version)
    -> bool {
  return isVersionInRange(schema_version, indexCatalogVersionRange());
}

auto IndexSchema::unsupportedVersionMessage(
    std::string_view schema_name, std::uint32_t schema_version,
    SchemaVersionRange version_range) -> std::string {
  std::ostringstream message_stream;
  message_stream << "unsupported " << schema_name << " format version "
                 << schema_version << " (supported ";
  if (version_range.minimumSupportedVersion == version_range.currentVersion) {
    message_stream << version_range.currentVersion;
  } else {
    message_stream << version_range.minimumSupportedVersion << '-'
                   << version_range.currentVersion;
  }
  message_stream << ')';
  return message_stream.str();
}

}  // namespace minisearch::index
