#pragma once

#include <filesystem>
#include <string>

#include "minisearch/config/AppConfig.hpp"

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

/**
 * @brief Parse MiniSearch command-line arguments and usage text.
 *
 * The parser resolves the requested command, target path, output file, and
 * worker count before the application hands control to the runtime mode.
 */
class CommandParser {
 public:
  /**
   * @brief Create a parser with runtime default options.
   *
   * @param app_config Runtime defaults used when options are omitted.
   */
  explicit CommandParser(config::AppConfig app_config = config::AppConfig{});

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

 private:
  config::AppConfig appConfig_;
};

}  // namespace minisearch::cli
