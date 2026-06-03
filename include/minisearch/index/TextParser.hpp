#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minisearch::index {

struct ParsedTerm {
  std::string term;
  std::uint32_t line = 0;
};

class TextParser {
 public:
  auto parseFile(const std::filesystem::path& path) const
      -> std::vector<ParsedTerm>;

  static auto tokenize(std::string_view text) -> std::vector<std::string>;
};

}  // namespace minisearch::index
