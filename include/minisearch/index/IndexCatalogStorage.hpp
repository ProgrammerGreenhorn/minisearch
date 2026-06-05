#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace minisearch::index {

struct ManagedIndex {
  std::string rootPath;
  std::filesystem::path indexFile;
  std::time_t updatedTime = 0;
};

class IndexCatalogStorage {
 public:
  /**
   * @brief Load recent-index metadata from a protobuf catalog file.
   *
   * @param catalog_file Input protobuf catalog path.
   * @return Recent indexes stored in the catalog.
   */
  auto load(const std::filesystem::path& catalog_file) const
      -> std::vector<ManagedIndex>;

  /**
   * @brief Save recent-index metadata to a protobuf catalog file.
   *
   * @param catalog_file Output protobuf catalog path.
   * @param managed_indexes Recent indexes to serialize.
   */
  auto save(const std::filesystem::path& catalog_file,
            const std::vector<ManagedIndex>& managed_indexes) const -> void;
};

}  // namespace minisearch::index
