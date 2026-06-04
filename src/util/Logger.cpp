#include "minisearch/util/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace minisearch::util {

namespace {

auto label(LogLevel log_level) -> const char* {
  switch (log_level) {
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

auto color(LogLevel log_level) -> const char* {
  switch (log_level) {
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
  const std::chrono::system_clock::time_point current_time_point =
      std::chrono::system_clock::now();
  const std::time_t current_time =
      std::chrono::system_clock::to_time_t(current_time_point);

  std::tm local_time{};
#ifdef _WIN32
  localtime_s(&local_time, &current_time);
#else
  localtime_r(&current_time, &local_time);
#endif

  std::ostringstream timestamp_stream;
  timestamp_stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
  return timestamp_stream.str();
}

}  // namespace

auto relativeSourcePath(const char* source_file) -> std::string {
  const std::string_view source_path(source_file);
  const std::string_view source_root(MINISEARCH_SOURCE_ROOT);
  if (source_path.size() > source_root.size() &&
      source_path.substr(0, source_root.size()) == source_root &&
      source_path[source_root.size()] == '/') {
    return std::string(source_path.substr(source_root.size() + 1));
  }

  return std::string(source_path);
}

auto Logger::log(LogLevel log_level, const std::string& message) -> void {
  std::ostream& output_stream =
      log_level == LogLevel::Error ? std::cerr : std::cout;
  output_stream << color(log_level) << '[' << timestamp() << "] ["
                << label(log_level) << "] " << message << "\033[0m\n";
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
