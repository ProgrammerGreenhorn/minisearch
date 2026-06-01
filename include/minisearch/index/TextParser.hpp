#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minisearch::index {

class TextParser {
public:
    std::vector<std::string> parseFile(const std::filesystem::path& path) const;
    static std::vector<std::string> tokenize(std::string_view text);
};

} // namespace minisearch::index

