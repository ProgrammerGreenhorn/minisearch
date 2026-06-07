#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "minisearch/config/AppConfig.hpp"

namespace {

using minisearch::config::SearchMode;

class ScopedTempDir {
 public:
  explicit ScopedTempDir(const std::string& name) {
    const auto unique_suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (name + "_" + std::to_string(unique_suffix));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ignored_error;
    std::filesystem::remove_all(path_, ignored_error);
  }

  auto path() const -> const std::filesystem::path& { return path_; }

 private:
  std::filesystem::path path_;
};

auto WriteFile(const std::filesystem::path& file_path,
               const std::string& contents) -> void {
  std::filesystem::create_directories(file_path.parent_path());
  std::ofstream output_stream(file_path);
  ASSERT_TRUE(output_stream);
  output_stream << contents;
}

TEST(AppConfigTest, MissingConfigUsesDefaults) {
  ScopedTempDir temp_dir("minisearch_config_missing");
  const auto app_config = minisearch::config::loadAppConfigFromFile(
      temp_dir.path() / "missing.toml");

  EXPECT_GT(app_config.threads, 0U);
  EXPECT_EQ(app_config.defaultSearchMode, SearchMode::FileNames);
  EXPECT_TRUE(app_config.openCurrentIndexOnStartup);
  EXPECT_TRUE(app_config.logFile.empty());
  EXPECT_FALSE(app_config.scannerOptions.excludedNames.empty());
}

TEST(AppConfigTest, LoadsTomlConfigValues) {
  ScopedTempDir temp_dir("minisearch_config_values");
  const auto config_file = temp_dir.path() / "config.toml";
  WriteFile(config_file, R"toml(
[index]
threads = 4
max_text_file_bytes = 4096
excluded_names = [".git", "out"]

[search]
default_mode = "grep"

[startup]
open_current_index = false

[logging]
file = "/tmp/minisearch-test.log"
)toml");

  const auto app_config =
      minisearch::config::loadAppConfigFromFile(config_file);

  EXPECT_EQ(app_config.threads, 4U);
  EXPECT_EQ(app_config.scannerOptions.maxTextFileBytes, 4096U);
  EXPECT_EQ(app_config.scannerOptions.excludedNames,
            (std::vector<std::string>{".git", "out"}));
  EXPECT_EQ(app_config.defaultSearchMode, SearchMode::GrepText);
  EXPECT_FALSE(app_config.openCurrentIndexOnStartup);
  EXPECT_EQ(app_config.logFile,
            std::filesystem::path("/tmp/minisearch-test.log"));
}

TEST(AppConfigTest, RejectsUnknownSearchMode) {
  ScopedTempDir temp_dir("minisearch_config_bad_mode");
  const auto config_file = temp_dir.path() / "config.toml";
  WriteFile(config_file, R"toml(
[search]
default_mode = "everything"
)toml");

  EXPECT_THROW(minisearch::config::loadAppConfigFromFile(config_file),
               std::runtime_error);
}

}  // namespace
