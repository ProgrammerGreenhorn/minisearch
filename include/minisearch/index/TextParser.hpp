#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minisearch::index {

class TextParser {
 public:
  auto parseFile(const std::filesystem::path& path) const
      -> std::vector<std::string>;

  static auto tokenize(std::string_view text) -> std::vector<std::string>;
};

}  // namespace minisearch::index
