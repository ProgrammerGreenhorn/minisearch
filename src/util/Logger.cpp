#include "minisearch/util/Logger.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace minisearch::util {

namespace {

constexpr std::string_view LogFileEnvironmentVariable = "MINISEARCH_LOG_FILE";

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

auto formattedLine(LogLevel log_level,
                   const std::string& message) -> std::string {
  std::ostringstream line_stream;
  line_stream << '[' << timestamp() << "] [" << label(log_level) << "] "
              << message;
  return line_stream.str();
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

auto Logger::instance() -> Logger& {
  static Logger logger;
  return logger;
}

Logger::~Logger() = default;

auto Logger::configureFromEnvironment() -> void {
  const char* log_file_value = std::getenv(LogFileEnvironmentVariable.data());
  if (log_file_value == nullptr || std::string_view(log_file_value).empty()) {
    clearLogFile();
    return;
  }

  setLogFile(log_file_value);
}

auto Logger::setLogFile(const std::filesystem::path& log_file) -> void {
  const std::filesystem::path parent_directory = log_file.parent_path();
  if (!parent_directory.empty()) {
    std::error_code create_error;
    std::filesystem::create_directories(parent_directory, create_error);
    if (create_error) {
      throw std::runtime_error("failed to create log directory: " +
                               parent_directory.string());
    }
  }

  auto next_file_stream =
      std::make_unique<std::ofstream>(log_file, std::ios::app);
  if (!*next_file_stream) {
    throw std::runtime_error("failed to open log file: " + log_file.string());
  }

  std::lock_guard<std::mutex> lock(mutex_);
  fileStream_ = std::move(next_file_stream);
}

auto Logger::clearLogFile() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  fileStream_.reset();
}

auto Logger::write(LogLevel log_level, const std::string& message) -> void {
  const std::string line_text = formattedLine(log_level, message);

  std::lock_guard<std::mutex> lock(mutex_);
  std::ostream& console_stream =
      log_level == LogLevel::Error ? std::cerr : std::cout;
  console_stream << color(log_level) << line_text << "\033[0m\n";

  if (fileStream_ != nullptr) {
    *fileStream_ << line_text << '\n';
    fileStream_->flush();
  }
}

auto Logger::log(LogLevel log_level, const std::string& message) -> void {
  write(log_level, message);
}

auto Logger::debug(const std::string& message) -> void {
#ifdef MINISEARCH_ENABLE_DEBUG_LOG
  write(LogLevel::Debug, message);
#else
  (void)message;
#endif
}

auto Logger::info(const std::string& message) -> void {
  write(LogLevel::Info, message);
}

auto Logger::warning(const std::string& message) -> void {
  write(LogLevel::Warning, message);
}

auto Logger::error(const std::string& message) -> void {
  write(LogLevel::Error, message);
}

}  // namespace minisearch::util
