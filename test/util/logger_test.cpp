#include "minisearch/util/Logger.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace {

using minisearch::util::Logger;

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

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const std::string& name, const std::string& value)
      : name_(name) {
    const char* previous_value = std::getenv(name_.c_str());
    if (previous_value != nullptr) {
      old_value_ = previous_value;
      had_old_value_ = true;
    }

    setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvironmentVariable() {
    if (had_old_value_) {
      setenv(name_.c_str(), old_value_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::string old_value_;
  bool had_old_value_ = false;
};

auto ReadFile(const std::filesystem::path& file_path) -> std::string {
  std::ifstream input_stream(file_path);
  EXPECT_TRUE(input_stream);

  std::ostringstream content_stream;
  content_stream << input_stream.rdbuf();
  return content_stream.str();
}

TEST(LoggerTest, InstanceReturnsSameLogger) {
  Logger& first_logger = Logger::instance();
  Logger& second_logger = Logger::instance();

  EXPECT_EQ(&first_logger, &second_logger);
}

TEST(LoggerTest, WritesPlainTextLogFile) {
  ScopedTempDir temp_dir("minisearch_logger_file");
  const auto log_file = temp_dir.path() / "nested" / "minisearch.log";

  Logger::instance().clearLogFile();
  Logger::instance().setLogFile(log_file);
  Logger::instance().info("logger file output test");
  Logger::instance().warning("logger warning output test");
  Logger::instance().clearLogFile();

  const std::string log_content = ReadFile(log_file);
  EXPECT_NE(log_content.find("[INFO ] logger file output test"),
            std::string::npos);
  EXPECT_NE(log_content.find("[WARN ] logger warning output test"),
            std::string::npos);
  EXPECT_EQ(log_content.find("\033"), std::string::npos);
}

TEST(LoggerTest, ConfigureFromEnvironmentEnablesFileOutput) {
  ScopedTempDir temp_dir("minisearch_logger_env");
  const auto log_file = temp_dir.path() / "minisearch.log";
  ScopedEnvironmentVariable scoped_log_file("MINISEARCH_LOG_FILE",
                                            log_file.string());

  Logger::instance().clearLogFile();
  Logger::instance().configureFromEnvironment();
  Logger::instance().error("logger env output test");
  Logger::instance().clearLogFile();

  const std::string log_content = ReadFile(log_file);
  EXPECT_NE(log_content.find("[ERROR] logger env output test"),
            std::string::npos);
}

}  // namespace
