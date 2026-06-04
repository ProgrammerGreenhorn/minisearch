#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace minisearch::util {

/**
 * @brief Hash a string with MiniSearch's stable hash algorithm.
 *
 * @param input_text String data to hash.
 * @return Stable 64-bit hash value.
 */
auto stableHash(std::string_view input_text) -> std::uint64_t;

/**
 * @brief Hash a file's byte contents with MiniSearch's stable hash algorithm.
 *
 * @param file_path File path to read.
 * @return Stable 64-bit hash value, or 0 if the file cannot be opened.
 */
auto stableFileHash(const std::filesystem::path& file_path) -> std::uint64_t;

/**
 * @brief Format a 64-bit hash as lowercase hexadecimal text.
 *
 * @param hash_value Hash value to format.
 * @return Sixteen-character hexadecimal representation.
 */
auto hashToHex(std::uint64_t hash_value) -> std::string;

}  // namespace minisearch::util
