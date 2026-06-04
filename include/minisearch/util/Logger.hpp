#pragma once

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
   * @brief Write a log message at the requested level.
   *
   * @param log_level Severity level for the message.
   * @param message Message text to write.
   */
  static auto log(LogLevel log_level, const std::string& message) -> void;

  /**
   * @brief Write a debug log message.
   *
   * @param message Message text to write.
   */
  static auto debug(const std::string& message) -> void;

  /**
   * @brief Write an info log message.
   *
   * @param message Message text to write.
   */
  static auto info(const std::string& message) -> void;

  /**
   * @brief Write a warning log message.
   *
   * @param message Message text to write.
   */
  static auto warning(const std::string& message) -> void;

  /**
   * @brief Write an error log message.
   *
   * @param message Message text to write.
   */
  static auto error(const std::string& message) -> void;
};

}  // namespace minisearch::util

#define MINISEARCH_LOG_LOCATION_MESSAGE(message)                           \
  (::std::string("[") + ::minisearch::util::relativeSourcePath(__FILE__) + \
   ":" + ::std::to_string(__LINE__) + "] [" + (message) + "]")

#ifdef MINISEARCH_ENABLE_DEBUG_LOG
#define MINISEARCH_LOG_DEBUG(message) \
  ::minisearch::util::Logger::debug(MINISEARCH_LOG_LOCATION_MESSAGE(message))
#else
#define MINISEARCH_LOG_DEBUG(message) ((void)0)
#endif

#define MINISEARCH_LOG_INFO(message) \
  ::minisearch::util::Logger::info(MINISEARCH_LOG_LOCATION_MESSAGE(message))

#define MINISEARCH_LOG_WARNING(message) \
  ::minisearch::util::Logger::warning(MINISEARCH_LOG_LOCATION_MESSAGE(message))

#define MINISEARCH_LOG_ERROR(message) \
  ::minisearch::util::Logger::error(MINISEARCH_LOG_LOCATION_MESSAGE(message))
