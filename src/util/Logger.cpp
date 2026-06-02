#include "minisearch/util/Logger.hpp"

#include <iostream>

namespace minisearch::util {

namespace {

auto label(LogLevel level) -> const char* {
  switch (level) {
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Info:
      return "info";
    case LogLevel::Warning:
      return "warning";
    case LogLevel::Error:
      return "error";
  }
  return "log";
}

auto color(LogLevel level) -> const char* {
  switch (level) {
    case LogLevel::Debug:
      return "\033[36m";
    case LogLevel::Info:
      return "\033[32m";
    case LogLevel::Warning:
      return "\033[33m";
    case LogLevel::Error:
      return "\033[31m";
  }
  return "\033[0m";
}

}  // namespace

auto Logger::log(LogLevel level, const std::string& message) -> void {
  auto& stream = level == LogLevel::Error ? std::cerr : std::cout;
  stream << color(level) << '[' << label(level) << "] " << message
         << "\033[0m\n";
}

auto Logger::debug(const std::string& message) -> void {
#ifdef MINISEARCH_ENABLE_DEBUG_LOG
  log(LogLevel::Debug, message);
#else
  (void)message;
#endif
}

auto Logger::info(const std::string& message) -> void {
  log(LogLevel::Info, message);
}

auto Logger::warning(const std::string& message) -> void {
  log(LogLevel::Warning, message);
}

auto Logger::error(const std::string& message) -> void {
  log(LogLevel::Error, message);
}

}  // namespace minisearch::util
