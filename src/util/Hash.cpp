#include "minisearch/util/Hash.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace minisearch::util {

namespace {

constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

auto updateFnv1a(std::uint64_t hash_value,
                 unsigned char byte_value) -> std::uint64_t {
  hash_value ^= byte_value;
  hash_value *= FnvPrime;
  return hash_value;
}

auto fnv1a(std::string_view input_text) -> std::uint64_t {
  std::uint64_t hash_value = FnvOffsetBasis;
  for (const auto character : input_text) {
    hash_value = updateFnv1a(hash_value, static_cast<unsigned char>(character));
  }
  return hash_value;
}

auto fnv1aFile(const std::filesystem::path& file_path) -> std::uint64_t {
  std::ifstream input_stream(file_path, std::ios::binary);
  if (!input_stream) {
    return 0;
  }

  std::uint64_t hash_value = FnvOffsetBasis;
  std::array<char, 4096> read_buffer{};
  while (input_stream.read(read_buffer.data(),
                           static_cast<std::streamsize>(read_buffer.size())) ||
         input_stream.gcount() > 0) {
    const std::streamsize bytes_read = input_stream.gcount();
    for (std::streamsize byte_index = 0; byte_index < bytes_read;
         ++byte_index) {
      hash_value = updateFnv1a(
          hash_value, static_cast<unsigned char>(
                          read_buffer[static_cast<std::size_t>(byte_index)]));
    }
  }
  return hash_value;
}

auto hex64(std::uint64_t hash_value) -> std::string {
  std::ostringstream hex_stream;
  hex_stream << std::hex << std::setw(16) << std::setfill('0') << hash_value;
  return hex_stream.str();
}

}  // namespace

auto stableHash(std::string_view input_text) -> std::uint64_t {
  return fnv1a(input_text);
}

auto stableFileHash(const std::filesystem::path& file_path) -> std::uint64_t {
  return fnv1aFile(file_path);
}

auto hashToHex(std::uint64_t hash_value) -> std::string {
  return hex64(hash_value);
}

}  // namespace minisearch::util
