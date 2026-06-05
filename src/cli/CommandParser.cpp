#include "minisearch/cli/CommandParser.hpp"

#include <stdexcept>
#include <thread>

#include "minisearch/index/IndexRepository.hpp"

namespace minisearch::cli {

namespace {

auto defaultThreadCount() -> std::size_t {
  const unsigned int hardware_thread_count =
      std::thread::hardware_concurrency();
  return hardware_thread_count == 0 ? 2 : hardware_thread_count;
}

auto resolveDefaultIndexFile(const std::filesystem::path& target_path)
    -> std::filesystem::path {
  return index::IndexRepository::indexFileForPath(target_path);
}

auto parseIndexOptions(CommandOptions& command_options, int argument_count,
                       char** argument_values, int option_start_index) -> void {
  for (int argument_index = option_start_index; argument_index < argument_count;
       ++argument_index) {
    const std::string option_text = argument_values[argument_index];
    if ((option_text == "--output" || option_text == "-o") &&
        argument_index + 1 < argument_count) {
      command_options.indexFile = argument_values[++argument_index];
      command_options.indexFileExplicit = true;
    } else if (option_text == "--threads" &&
               argument_index + 1 < argument_count) {
      command_options.threads = static_cast<std::size_t>(
          std::stoul(argument_values[++argument_index]));
      if (command_options.threads == 0) {
        command_options.threads = defaultThreadCount();
      }
    } else {
      throw std::invalid_argument("unknown option: " + option_text);
    }
  }
}

auto resolveIndexFile(CommandOptions& command_options) -> void {
  if (!command_options.indexFileExplicit) {
    command_options.indexFile =
        resolveDefaultIndexFile(command_options.targetPath);
  }
}

}  // namespace

auto CommandParser::parse(int argument_count,
                          char** argument_values) const -> CommandOptions {
  CommandOptions command_options;
  command_options.threads = defaultThreadCount();

  if (argument_count <= 1) {
    command_options.command = Command::OpenCurrent;
    return command_options;
  }

  const std::string command_text = argument_values[1];
  if (command_text == "help" || command_text == "--help" ||
      command_text == "-h") {
    command_options.command = Command::Help;
    return command_options;
  }

  if (!command_text.empty() && command_text[0] == '-') {
    command_options.command = Command::Unknown;
    return command_options;
  }

  command_options.command = Command::Index;
  command_options.targetPath = argument_values[1];
  command_options.targetPathExplicit = true;
  parseIndexOptions(command_options, argument_count, argument_values, 2);
  resolveIndexFile(command_options);
  return command_options;
}

auto CommandParser::helpText() -> std::string {
  return R"(MiniSearch

Usage:
  minisearch
  minisearch <path> [--output <file>] [--threads <n>]
  minisearch help

Inside the shell:
  index <path>   Build or refresh an index
  recent         Show recently indexed roots
  find <query>   Search file names and paths
  grep <query>   Search indexed text content

Examples:
  minisearch
  (minisearch) index ./src
  (minisearch) recent
)";
}

}  // namespace minisearch::cli
