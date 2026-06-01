#pragma once

#include <filesystem>
#include <string>

namespace minisearch::cli {

enum class Command {
    Help,
    Index,
    Find,
    Grep,
    Stats,
    Unknown
};

struct CommandOptions {
    Command command = Command::Help;
    std::filesystem::path targetPath;
    std::filesystem::path indexFile = ".minisearch.index";
    std::string query;
    std::size_t threads = 0;
};

class CommandParser {
public:
    CommandOptions parse(int argc, char** argv) const;
    static std::string helpText();
};

} // namespace minisearch::cli

