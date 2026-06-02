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
