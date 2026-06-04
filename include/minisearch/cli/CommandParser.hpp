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
  /**
   * @brief Parse command-line arguments into command options.
   *
   * @param argument_count Number of command-line arguments.
   * @param argument_values Command-line argument vector.
   * @return Parsed command options.
   */
  auto parse(int argument_count,
             char** argument_values) const -> CommandOptions;

  /**
   * @brief Build the command-line help text.
   *
   * @return Usage text shown for help and invalid commands.
   */
  static auto helpText() -> std::string;
};

}  // namespace minisearch::cli
