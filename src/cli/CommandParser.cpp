#include "minisearch/cli/CommandParser.hpp"

#include <stdexcept>
#include <thread>

#include "minisearch/index/IndexRepository.hpp"

namespace minisearch::cli {

namespace {

auto defaultThreadCount() -> std::size_t {
  const unsigned int hardware = std::thread::hardware_concurrency();
  return hardware == 0 ? 2 : hardware;
}

auto resolveDefaultIndexFile(const std::filesystem::path& path)
    -> std::filesystem::path {
  return index::IndexRepository::indexFileForPath(path);
}

auto parseIndexOptions(CommandOptions& options, int argc, char** argv,
                       int startIndex) -> void {
  for (int i = startIndex; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
      options.indexFile = argv[++i];
      options.indexFileExplicit = true;
    } else if (arg == "--threads" && i + 1 < argc) {
      options.threads = static_cast<std::size_t>(std::stoul(argv[++i]));
      if (options.threads == 0) {
        options.threads = defaultThreadCount();
      }
    } else {
      throw std::invalid_argument("unknown option: " + arg);
    }
  }
}

auto resolveIndexFile(CommandOptions& options) -> void {
  if (!options.indexFileExplicit) {
    options.indexFile = resolveDefaultIndexFile(options.targetPath);
  }
}

}  // namespace

auto CommandParser::parse(int argc, char** argv) const -> CommandOptions {
  CommandOptions options;
  options.threads = defaultThreadCount();

  if (argc <= 1) {
    options.command = Command::OpenCurrent;
    return options;
  }

  const std::string command = argv[1];
  if (command == "help" || command == "--help" || command == "-h") {
    options.command = Command::Help;
    return options;
  }

  if (!command.empty() && command[0] == '-') {
    options.command = Command::Unknown;
    return options;
  }

  options.command = Command::Index;
  options.targetPath = argv[1];
  options.targetPathExplicit = true;
  parseIndexOptions(options, argc, argv, 2);
  resolveIndexFile(options);
  return options;
}

auto CommandParser::helpText() -> std::string {
  return R"(MiniSearch

Usage:
  minisearch
  minisearch <path> [--output <file>] [--threads <n>]
  minisearch help

Examples:
  minisearch .
  minisearch ./src
  minisearch
)";
}

}  // namespace minisearch::cli
