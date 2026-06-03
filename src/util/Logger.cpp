#include "minisearch/util/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace minisearch::util {

namespace {

auto label(LogLevel level) -> const char* {
  switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO ";
    case LogLevel::Warning:
      return "WARN ";
    case LogLevel::Error:
      return "ERROR";
  }
  return "LOG  ";
}

auto color(LogLevel level) -> const char* {
  switch (level) {
    case LogLevel::Debug:
      return "\033[35m";
    case LogLevel::Info:
      return "\033[32m";
    case LogLevel::Warning:
      return "\033[33m";
    case LogLevel::Error:
      return "\033[31m";
  }
  return "\033[0m";
}

auto timestamp() -> std::string {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);

  std::tm localTime{};
#ifdef _WIN32
  localtime_s(&localTime, &time);
#else
  localtime_r(&time, &localTime);
#endif

  std::ostringstream stream;
  stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

}  // namespace

auto relativeSourcePath(const char* file) -> std::string {
  const std::string_view path(file);
  const std::string_view root(MINISEARCH_SOURCE_ROOT);
  if (path.size() > root.size() && path.substr(0, root.size()) == root &&
      path[root.size()] == '/') {
    return std::string(path.substr(root.size() + 1));
  }

  return std::string(path);
}

auto Logger::log(LogLevel level, const std::string& message) -> void {
  auto& stream = level == LogLevel::Error ? std::cerr : std::cout;
  stream << color(level) << '[' << timestamp() << "] [" << label(level) << "] "
         << message << "\033[0m\n";
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
