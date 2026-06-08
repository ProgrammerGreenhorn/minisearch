#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "minisearch/config/AppConfig.hpp"
#include "minisearch/util/Logger.hpp"

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

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(std::string name, std::string value)
      : name_(std::move(name)) {
    const char* previous_value = std::getenv(name_.c_str());
    if (previous_value != nullptr) {
      oldValue_ = previous_value;
      hadOldValue_ = true;
    }

    setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvironmentVariable() {
    if (hadOldValue_) {
      setenv(name_.c_str(), oldValue_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::string oldValue_;
  bool hadOldValue_ = false;
};

class ScopedUnsetEnvironmentVariable {
 public:
  explicit ScopedUnsetEnvironmentVariable(std::string name)
      : name_(std::move(name)) {
    const char* previous_value = std::getenv(name_.c_str());
    if (previous_value != nullptr) {
      oldValue_ = previous_value;
      hadOldValue_ = true;
    }

    unsetenv(name_.c_str());
  }

  ~ScopedUnsetEnvironmentVariable() {
    if (hadOldValue_) {
      setenv(name_.c_str(), oldValue_.c_str(), 1);
    }
  }

 private:
  std::string name_;
  std::string oldValue_;
  bool hadOldValue_ = false;
};

auto ReadFile(const std::filesystem::path& file_path) -> std::string {
  std::ifstream input_stream(file_path);
  EXPECT_TRUE(input_stream);

  std::ostringstream content_stream;
  content_stream << input_stream.rdbuf();
  return content_stream.str();
}

TEST(AppConfigTest, MissingConfigUsesDefaults) {
  ScopedTempDir temp_dir("minisearch_config_missing");
  const auto app_config = minisearch::config::loadAppConfigFromFile(
      temp_dir.path() / "missing.toml");

  EXPECT_GT(app_config.threads, 0U);
  EXPECT_EQ(app_config.defaultSearchMode, SearchMode::FileNames);
  EXPECT_TRUE(app_config.openCurrentIndexOnStartup);
  EXPECT_TRUE(app_config.logFile.empty());
  EXPECT_TRUE(app_config.scannerOptions.excludedNames.empty());
}

TEST(AppConfigTest, CreatesDefaultConfigFileWhenMissing) {
  ScopedTempDir temp_dir("minisearch_config_create_default");
  const auto config_file = temp_dir.path() / "nested" / "config.toml";

  EXPECT_TRUE(minisearch::config::ensureDefaultConfigFile(config_file));
  EXPECT_TRUE(std::filesystem::exists(config_file));

  const std::string config_content = ReadFile(config_file);
  EXPECT_NE(config_content.find("[index]"), std::string::npos);
  EXPECT_NE(config_content.find("threads = 0"), std::string::npos);
  EXPECT_NE(config_content.find("excluded_names = []"), std::string::npos);
  EXPECT_NE(config_content.find("# file = \"/tmp/minisearch.log\""),
            std::string::npos);

  const auto app_config =
      minisearch::config::loadAppConfigFromFile(config_file);
  EXPECT_GT(app_config.threads, 0U);
  EXPECT_TRUE(app_config.logFile.empty());
  EXPECT_TRUE(app_config.scannerOptions.excludedNames.empty());
}

TEST(AppConfigTest, DoesNotOverwriteExistingConfigFile) {
  ScopedTempDir temp_dir("minisearch_config_keep_existing");
  const auto config_file = temp_dir.path() / "config.toml";
  const std::string existing_config = R"toml([index]
threads = 3
excluded_names = ["custom"]
)toml";
  WriteFile(config_file, existing_config);

  EXPECT_FALSE(minisearch::config::ensureDefaultConfigFile(config_file));
  EXPECT_EQ(ReadFile(config_file), existing_config);
}

TEST(AppConfigTest, LoadsTomlConfigValues) {
  ScopedTempDir temp_dir("minisearch_config_values");
  const auto config_file = temp_dir.path() / "config.toml";
  WriteFile(config_file, R"toml(
[index]
threads = 4
max_text_file_bytes = 4096
text_probe_bytes = 256
binary_control_ratio = 0.10
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
  EXPECT_EQ(app_config.scannerOptions.textProbeBytes, 256U);
  EXPECT_DOUBLE_EQ(app_config.scannerOptions.binaryControlRatio, 0.10);
  EXPECT_EQ(app_config.scannerOptions.excludedNames,
            (std::vector<std::string>{".git", "out"}));
  EXPECT_EQ(app_config.defaultSearchMode, SearchMode::GrepText);
  EXPECT_FALSE(app_config.openCurrentIndexOnStartup);
  EXPECT_EQ(app_config.logFile,
            std::filesystem::path("/tmp/minisearch-test.log"));
}

TEST(AppConfigTest, ConfigureLoggerUsesConfigLogFile) {
  ScopedUnsetEnvironmentVariable scoped_log_file("MINISEARCH_LOG_FILE");
  ScopedTempDir temp_dir("minisearch_config_logger_file");
  const auto log_file = temp_dir.path() / "minisearch.log";

  minisearch::config::AppConfig app_config;
  app_config.logFile = log_file;

  minisearch::util::Logger::instance().clearLogFile();
  minisearch::config::configureLogger(app_config);
  testing::internal::CaptureStdout();
  minisearch::util::Logger::instance().info("config logger output test");
  minisearch::util::Logger::instance().flush();
  (void)testing::internal::GetCapturedStdout();

  const std::string log_content = ReadFile(log_file);
  minisearch::util::Logger::instance().clearLogFile();
  EXPECT_NE(log_content.find("[INFO ] config logger output test"),
            std::string::npos);
}

TEST(AppConfigTest, ConfigureLoggerEnvironmentOverridesConfigLogFile) {
  ScopedTempDir temp_dir("minisearch_config_logger_env");
  const auto config_log_file = temp_dir.path() / "config.log";
  const auto env_log_file = temp_dir.path() / "env.log";
  ScopedEnvironmentVariable scoped_log_file("MINISEARCH_LOG_FILE",
                                            env_log_file.string());

  minisearch::config::AppConfig app_config;
  app_config.logFile = config_log_file;

  minisearch::util::Logger::instance().clearLogFile();
  minisearch::config::configureLogger(app_config);
  testing::internal::CaptureStderr();
  minisearch::util::Logger::instance().error("env logger override test");
  minisearch::util::Logger::instance().flush();
  (void)testing::internal::GetCapturedStderr();

  const std::string env_log_content = ReadFile(env_log_file);
  minisearch::util::Logger::instance().clearLogFile();
  EXPECT_NE(env_log_content.find("[ERROR] env logger override test"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(config_log_file));
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
