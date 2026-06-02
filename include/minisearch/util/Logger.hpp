#pragma once

#include <string>

namespace minisearch::util {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
 public:
  static auto log(LogLevel level, const std::string& message) -> void;

  static auto debug(const std::string& message) -> void;

  static auto info(const std::string& message) -> void;

  static auto warning(const std::string& message) -> void;

  static auto error(const std::string& message) -> void;
};

}  // namespace minisearch::util

#define MINISEARCH_LOG_LOCATION_MESSAGE(message)                         \
  (::std::string("[") + __FILE__ + ":" + ::std::to_string(__LINE__) +    \
   "] " + (message))

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
