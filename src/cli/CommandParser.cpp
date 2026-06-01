#include "minisearch/cli/CommandParser.hpp"

#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace minisearch::cli {

namespace {

std::string joinArgs(const std::vector<std::string>& args)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            stream << ' ';
        }
        stream << args[i];
    }
    return stream.str();
}

std::size_t defaultThreadCount()
{
    const auto hardware = std::thread::hardware_concurrency();
    return hardware == 0 ? 2 : hardware;
}

} // namespace

CommandOptions CommandParser::parse(int argc, char** argv) const
{
    CommandOptions options;
    options.threads = defaultThreadCount();

    if (argc <= 1) {
        options.command = Command::Help;
        return options;
    }

    const std::string command = argv[1];
    if (command == "help" || command == "--help" || command == "-h") {
        options.command = Command::Help;
        return options;
    }

    if (command == "index") {
        if (argc < 3) {
            throw std::invalid_argument("index command requires a target path");
        }

        options.command = Command::Index;
        options.targetPath = argv[2];

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
                options.indexFile = argv[++i];
            } else if (arg == "--threads" && i + 1 < argc) {
                options.threads = static_cast<std::size_t>(std::stoul(argv[++i]));
                if (options.threads == 0) {
                    options.threads = defaultThreadCount();
                }
            } else {
                throw std::invalid_argument("unknown index option: " + arg);
            }
        }

        return options;
    }

    if (command == "find" || command == "grep") {
        options.command = command == "find" ? Command::Find : Command::Grep;

        std::vector<std::string> queryParts;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if ((arg == "--index" || arg == "-i") && i + 1 < argc) {
                options.indexFile = argv[++i];
            } else {
                queryParts.push_back(arg);
            }
        }

        options.query = joinArgs(queryParts);
        if (options.query.empty()) {
            throw std::invalid_argument(command + " command requires a query");
        }

        return options;
    }

    if (command == "stats") {
        options.command = Command::Stats;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if ((arg == "--index" || arg == "-i") && i + 1 < argc) {
                options.indexFile = argv[++i];
            } else {
                throw std::invalid_argument("unknown stats option: " + arg);
            }
        }
        return options;
    }

    options.command = Command::Unknown;
    return options;
}

std::string CommandParser::helpText()
{
    return R"(MiniSearch

Usage:
  minisearch index <path> [--output <file>] [--threads <n>]
  minisearch find <query> [--index <file>]
  minisearch grep <query> [--index <file>]
  minisearch stats [--index <file>]
  minisearch help

Examples:
  minisearch index ./src
  minisearch find main
  minisearch grep "class parser"
)";
}

} // namespace minisearch::cli

