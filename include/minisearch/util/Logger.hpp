#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>

namespace minisearch::util {

enum class LogLevel { Debug, Info, Warning, Error };

/**
 * @brief Convert an absolute source file path to a project-relative path.
 *
 * @param source_file Source file path, usually from __FILE__.
 * @return Project-relative path when possible, otherwise the input path.
 */
auto relativeSourcePath(const char* source_file) -> std::string;

class Logger {
 public:
  /**
   * @brief Get the process-wide logger instance.
   *
   * @return Shared logger instance.
   */
  static auto instance() -> Logger&;

  /**
   * @brief Destroy the logger and close any configured file output.
   */
  ~Logger();

  Logger(const Logger&) = delete;
  auto operator=(const Logger&) -> Logger& = delete;
  Logger(Logger&&) = delete;
  auto operator=(Logger&&) -> Logger& = delete;

  /**
   * @brief Configure file output from MINISEARCH_LOG_FILE.
   */
  auto configureFromEnvironment() -> void;

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
   * @brief Write a log message at the requested level.
   *
   * @param log_level Severity level for the message.
   * @param message Message text to write.
   */
  auto log(LogLevel log_level, const std::string& message) -> void;

  /**
   * @brief Write a debug log message.
   *
   * @param message Message text to write.
   */
  auto debug(const std::string& message) -> void;

  /**
   * @brief Write an info log message.
   *
   * @param message Message text to write.
   */
  auto info(const std::string& message) -> void;

  /**
   * @brief Write a warning log message.
   *
   * @param message Message text to write.
   */
  auto warning(const std::string& message) -> void;

  /**
   * @brief Write an error log message.
   *
   * @param message Message text to write.
   */
  auto error(const std::string& message) -> void;

 private:
  Logger() = default;

  /**
   * @brief Write a formatted log message to all configured outputs.
   *
   * @param log_level Severity level for the message.
   * @param message Message text to write.
   */
  auto write(LogLevel log_level, const std::string& message) -> void;

  std::mutex mutex_;
  std::unique_ptr<std::ofstream> fileStream_;
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
