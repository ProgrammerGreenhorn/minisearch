#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace minisearch::util {

auto stableHash(std::string_view value) -> std::uint64_t;

auto stableFileHash(const std::filesystem::path& path) -> std::uint64_t;

auto hashToHex(std::uint64_t value) -> std::string;

}  // namespace minisearch::util
