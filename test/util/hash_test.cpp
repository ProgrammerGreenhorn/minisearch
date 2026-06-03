#include "minisearch/util/Hash.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

class ScopedTempDir {
 public:
  explicit ScopedTempDir(const std::string& name) {
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (name + "_" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  auto path() const -> const std::filesystem::path& { return path_; }

 private:
  std::filesystem::path path_;
};

TEST(HashTest, StableHashUsesKnownFnv1aValues) {
  EXPECT_EQ(minisearch::util::stableHash(""), 14695981039346656037ULL);
  EXPECT_EQ(minisearch::util::stableHash("hello"), 11831194018420276491ULL);
}

TEST(HashTest, HashToHexPadsToSixteenLowercaseDigits) {
  EXPECT_EQ(minisearch::util::hashToHex(0x2a), "000000000000002a");
}

TEST(HashTest, StableFileHashMatchesStableHashOfFileContents) {
  ScopedTempDir temp("minisearch_hash_file");
  const auto file = temp.path() / "data.txt";
  {
    std::ofstream output(file);
    ASSERT_TRUE(output);
    output << "hello";
  }

  EXPECT_EQ(minisearch::util::stableFileHash(file),
            minisearch::util::stableHash("hello"));
}

TEST(HashTest, StableFileHashReturnsZeroForMissingFile) {
  ScopedTempDir temp("minisearch_hash_missing");

  EXPECT_EQ(minisearch::util::stableFileHash(temp.path() / "missing"), 0U);
}

}  // namespace
