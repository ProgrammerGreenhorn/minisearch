#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace minisearch::util {

enum class LogLevel { Debug, Info, Warning, Error };

/**
 * @brief Convert an absolute source file path to a project-relative path.
 *
 * @param source_file Source file path, usually from __FILE__.
 * @return Project-relative path when possible, otherwise the input path.
 */
auto relativeSourcePath(const char* source_file) -> std::string;

/**
 * @brief Provide the process-wide asynchronous logger and file sink.
 *
 * Logger formats log messages, queues them on a background worker, and writes
 * them to stdout/stderr and an optional log file.
 */
class Logger {
 public:
  /**
   * @brief Get the process-wide logger instance.
   *
   * @return Shared logger instance.
   */
  static auto instance() -> Logger&;

  /**
   * @brief Stop the worker thread after flushing queued log messages.
   */
  ~Logger();

  Logger(const Logger&) = delete;
  auto operator=(const Logger&) -> Logger& = delete;
  Logger(Logger&&) = delete;
  auto operator=(Logger&&) -> Logger& = delete;

  /**
   * @brief Enable log-file output.
   *
   * @param log_file File path to append log messages to.
   */
  auto setLogFile(const std::filesystem::path& log_file) -> void;

  /**
   * @brief Disable log-file output and close the file stream.
   */
  auto clearLogFile() -> void;

  /**
   * @brief Wait until all queued log messages have been written.
   */
  auto flush() -> void;

  /**
   * @brief Queue a log message at the requested level.
   *
   * @param log_level Severity level for the message.
   * @param message Message text to write.
   */
  auto log(LogLevel log_level, const std::string& message) -> void;

  /**
   * @brief Queue a debug log message.
   *
   * @param message Message text to write.
   */
  auto debug(const std::string& message) -> void;

  /**
   * @brief Queue an info log message.
   *
   * @param message Message text to write.
   */
  auto info(const std::string& message) -> void;

  /**
   * @brief Queue a warning log message.
   *
   * @param message Message text to write.
   */
  auto warning(const std::string& message) -> void;

  /**
   * @brief Queue an error log message.
   *
   * @param message Message text to write.
   */
  auto error(const std::string& message) -> void;

 private:
  struct LogEntry {
    LogLevel log_level = LogLevel::Info;
    std::string line_text;
  };

  /**
   * @brief Start the background logger worker.
   */
  Logger();

  /**
   * @brief Add a formatted log entry to the worker queue.
   *
   * @param log_level Severity level for the message.
   * @param message Message text to write.
   */
  auto enqueue(LogLevel log_level, const std::string& message) -> void;

  /**
   * @brief Run the background worker loop.
   */
  auto workerLoop() -> void;

  /**
   * @brief Write a batch of formatted entries to configured outputs.
   *
   * @param log_entries Formatted log entries to write.
   */
  auto writeEntries(const std::deque<LogEntry>& log_entries) -> void;

  std::mutex queueMutex_;
  std::condition_variable queueCondition_;
  std::condition_variable flushCondition_;
  std::deque<LogEntry> logQueue_;
  std::size_t pendingEntries_ = 0;
  bool stopRequested_ = false;

  std::mutex outputMutex_;
  std::unique_ptr<std::ofstream> fileStream_;
  std::thread workerThread_;
};

}  // namespace minisearch::util

#define MINISEARCH_LOG_LOCATION_MESSAGE(message)                           \
  (::std::string("[") + ::minisearch::util::relativeSourcePath(__FILE__) + \
   ":" + ::std::to_string(__LINE__) + "] [" + (message) + "]")

#ifdef MINISEARCH_ENABLE_DEBUG_LOG
#define MINISEARCH_LOG_DEBUG(message)           \
  ::minisearch::util::Logger::instance().debug( \
      MINISEARCH_LOG_LOCATION_MESSAGE(message))
#else
#define MINISEARCH_LOG_DEBUG(message) ((void)0)
#endif

#define MINISEARCH_LOG_INFO(message)           \
  ::minisearch::util::Logger::instance().info( \
      MINISEARCH_LOG_LOCATION_MESSAGE(message))

#define MINISEARCH_LOG_WARNING(message)           \
  ::minisearch::util::Logger::instance().warning( \
      MINISEARCH_LOG_LOCATION_MESSAGE(message))

#define MINISEARCH_LOG_ERROR(message)           \
  ::minisearch::util::Logger::instance().error( \
      MINISEARCH_LOG_LOCATION_MESSAGE(message))
