#pragma once

#include <filesystem>
#include <string>

namespace minisearch::cli {

enum class Command { OpenCurrent, Help, Index, Unknown };

struct CommandOptions {
  Command command = Command::Help;
  std::filesystem::path targetPath = ".";
  std::filesystem::path indexFile;
  bool indexFileExplicit = false;
  bool targetPathExplicit = false;
  std::size_t threads = 0;
};

class CommandParser {
 public:
  auto parse(int argc, char** argv) const -> CommandOptions;
  static auto helpText() -> std::string;
};

}  // namespace minisearch::cli
