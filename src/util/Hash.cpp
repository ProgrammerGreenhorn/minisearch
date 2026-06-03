#include "minisearch/util/Hash.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace minisearch::util {

namespace {

constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

auto updateFnv1a(std::uint64_t hash, unsigned char value) -> std::uint64_t {
  hash ^= value;
  hash *= FnvPrime;
  return hash;
}

auto fnv1a(std::string_view value) -> std::uint64_t {
  std::uint64_t hash = FnvOffsetBasis;
  for (const auto ch : value) {
    hash = updateFnv1a(hash, static_cast<unsigned char>(ch));
  }
  return hash;
}

auto fnv1aFile(const std::filesystem::path& path) -> std::uint64_t {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return 0;
  }

  std::uint64_t hash = FnvOffsetBasis;
  std::array<char, 4096> buffer{};
  while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) ||
         input.gcount() > 0) {
    const std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      hash = updateFnv1a(
          hash, static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]));
    }
  }
  return hash;
}

auto hex64(std::uint64_t value) -> std::string {
  std::ostringstream stream;
  stream << std::hex << std::setw(16) << std::setfill('0') << value;
  return stream.str();
}

}  // namespace

auto stableHash(std::string_view value) -> std::uint64_t {
  return fnv1a(value);
}

auto stableFileHash(const std::filesystem::path& path) -> std::uint64_t {
  return fnv1aFile(path);
}

auto hashToHex(std::uint64_t value) -> std::string {
  return hex64(value);
}

}  // namespace minisearch::util
