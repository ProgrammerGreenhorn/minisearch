#include "minisearch/util/Logger.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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

auto CountOccurrences(const std::string& text,
                      const std::string& pattern) -> std::size_t {
  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = text.find(pattern, offset)) != std::string::npos) {
    ++count;
    offset += pattern.size();
  }
  return count;
}

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
  testing::internal::CaptureStdout();
  Logger::instance().info("logger file output test");
  Logger::instance().warning("logger warning output test");
  Logger::instance().flush();
  (void)testing::internal::GetCapturedStdout();

  const std::string log_content = ReadFile(log_file);
  Logger::instance().clearLogFile();
  EXPECT_NE(log_content.find("[INFO ] logger file output test"),
            std::string::npos);
  EXPECT_NE(log_content.find("[WARN ] logger warning output test"),
            std::string::npos);
  EXPECT_EQ(log_content.find("\033"), std::string::npos);
}

TEST(LoggerTest, FlushWaitsForQueuedMessages) {
  ScopedTempDir temp_dir("minisearch_logger_flush");
  const auto log_file = temp_dir.path() / "minisearch.log";

  Logger::instance().clearLogFile();
  Logger::instance().setLogFile(log_file);
  testing::internal::CaptureStdout();
  for (int message_index = 0; message_index < 10; ++message_index) {
    Logger::instance().info("flush queued message");
  }
  Logger::instance().flush();
  (void)testing::internal::GetCapturedStdout();

  const std::string log_content = ReadFile(log_file);
  Logger::instance().clearLogFile();

  EXPECT_EQ(CountOccurrences(log_content, "flush queued message"), 10U);
}

TEST(LoggerTest, FlushWaitsForLargeQueuedBatch) {
  ScopedTempDir temp_dir("minisearch_logger_batch");
  const auto log_file = temp_dir.path() / "minisearch.log";
  constexpr int message_count = 250;

  Logger::instance().clearLogFile();
  Logger::instance().setLogFile(log_file);
  testing::internal::CaptureStdout();
  for (int message_index = 0; message_index < message_count; ++message_index) {
    Logger::instance().info("batched logger message");
  }
  Logger::instance().flush();
  (void)testing::internal::GetCapturedStdout();

  const std::string log_content = ReadFile(log_file);
  Logger::instance().clearLogFile();

  EXPECT_EQ(CountOccurrences(log_content, "batched logger message"),
            static_cast<std::size_t>(message_count));
}

TEST(LoggerTest, WritesConcurrentMessages) {
  ScopedTempDir temp_dir("minisearch_logger_threads");
  const auto log_file = temp_dir.path() / "minisearch.log";
  constexpr int thread_count = 4;
  constexpr int messages_per_thread = 25;

  Logger::instance().clearLogFile();
  Logger::instance().setLogFile(log_file);

  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  testing::internal::CaptureStdout();
  for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
    threads.emplace_back([messages_per_thread]() {
      for (int message_index = 0; message_index < messages_per_thread;
           ++message_index) {
        Logger::instance().info("threaded logger message");
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
  Logger::instance().flush();
  (void)testing::internal::GetCapturedStdout();

  const std::string log_content = ReadFile(log_file);
  Logger::instance().clearLogFile();

  EXPECT_EQ(CountOccurrences(log_content, "threaded logger message"),
            static_cast<std::size_t>(thread_count * messages_per_thread));
}

}  // namespace
