#include "minisearch/util/Logger.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

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

Logger::Logger() : workerThread_(&Logger::workerLoop, this) {}

Logger::~Logger() {
  flush();
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    stopRequested_ = true;
  }
  queueCondition_.notify_one();

  if (workerThread_.joinable()) {
    workerThread_.join();
  }

  std::lock_guard<std::mutex> output_lock(outputMutex_);
  fileStream_.reset();
}

auto Logger::setLogFile(const std::filesystem::path& log_file) -> void {
  flush();

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

  std::lock_guard<std::mutex> output_lock(outputMutex_);
  fileStream_ = std::move(next_file_stream);
}

auto Logger::clearLogFile() -> void {
  flush();

  std::lock_guard<std::mutex> output_lock(outputMutex_);
  fileStream_.reset();
}

auto Logger::flush() -> void {
  {
    std::unique_lock<std::mutex> queue_lock(queueMutex_);
    flushCondition_.wait(queue_lock, [this]() { return pendingEntries_ == 0; });
  }

  std::lock_guard<std::mutex> output_lock(outputMutex_);
  std::cout.flush();
  std::cerr.flush();
  if (fileStream_ != nullptr) {
    fileStream_->flush();
  }
}

auto Logger::enqueue(LogLevel log_level, const std::string& message) -> void {
  LogEntry log_entry{log_level, formattedLine(log_level, message)};
  {
    std::lock_guard<std::mutex> queue_lock(queueMutex_);
    if (stopRequested_) {
      return;
    }

    logQueue_.push_back(std::move(log_entry));
    ++pendingEntries_;
  }

  queueCondition_.notify_one();
}

auto Logger::workerLoop() -> void {
  while (true) {
    std::deque<LogEntry> log_entries;
    {
      std::unique_lock<std::mutex> queue_lock(queueMutex_);
      queueCondition_.wait(queue_lock, [this]() {
        return stopRequested_ || !logQueue_.empty();
      });

      if (logQueue_.empty()) {
        if (stopRequested_) {
          return;
        }
        continue;
      }

      log_entries.swap(logQueue_);
    }

    writeEntries(log_entries);

    {
      std::lock_guard<std::mutex> queue_lock(queueMutex_);
      pendingEntries_ -= log_entries.size();
      if (pendingEntries_ == 0) {
        flushCondition_.notify_all();
      }
    }
  }
}

auto Logger::writeEntries(const std::deque<LogEntry>& log_entries) -> void {
  if (log_entries.empty()) {
    return;
  }

  std::lock_guard<std::mutex> output_lock(outputMutex_);
  std::ostringstream file_output_buffer;
  for (const auto& log_entry : log_entries) {
    std::ostream& console_stream =
        log_entry.log_level == LogLevel::Error ? std::cerr : std::cout;
    console_stream << color(log_entry.log_level) << log_entry.line_text
                   << "\033[0m\n";

    if (fileStream_ != nullptr) {
      file_output_buffer << log_entry.line_text << '\n';
    }
  }

  if (fileStream_ != nullptr) {
    *fileStream_ << file_output_buffer.str();
  }
}

auto Logger::log(LogLevel log_level, const std::string& message) -> void {
  enqueue(log_level, message);
}

auto Logger::debug(const std::string& message) -> void {
#ifdef MINISEARCH_ENABLE_DEBUG_LOG
  enqueue(LogLevel::Debug, message);
#else
  (void)message;
#endif
}

auto Logger::info(const std::string& message) -> void {
  enqueue(LogLevel::Info, message);
}

auto Logger::warning(const std::string& message) -> void {
  enqueue(LogLevel::Warning, message);
}

auto Logger::error(const std::string& message) -> void {
  enqueue(LogLevel::Error, message);
}

}  // namespace minisearch::util
