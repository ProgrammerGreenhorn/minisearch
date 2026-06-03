#include "minisearch/shell/InteractiveShell.hpp"

#include <cctype>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include "minisearch/search/SearchEngine.hpp"

namespace minisearch::shell {

namespace {

auto trim(std::string value) -> std::string {
  auto first = value.begin();
  while (first != value.end() &&
         std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }

  auto last = value.end();
  while (last != first &&
         std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }

  return std::string(first, last);
}

auto splitCommand(const std::string& line)
    -> std::pair<std::string, std::string> {
  std::istringstream stream(line);
  std::string command;
  stream >> command;

  std::string rest;
  std::getline(stream, rest);
  return {command, trim(rest)};
}

auto printRecords(const std::vector<index::FileRecord>& records) -> void {
  for (const auto& record : records) {
    std::cout << search::SearchEngine::formatRecord(record) << '\n';
  }
  std::cout << records.size() << " result(s)\n";
}

auto printGrepMatches(
    const std::vector<search::SearchEngine::GrepLine>& matches) -> void {
  for (std::size_t index = 0; index < matches.size(); ++index) {
    std::cout << search::SearchEngine::formatGrepLine(matches[index]) << '\n';
    if (index + 1 < matches.size()) {
      std::cout << '\n';
    }
  }
  std::cout << matches.size() << " line(s)\n";
}

}  // namespace

InteractiveShell::InteractiveShell(index::InvertedIndex index,
                                   std::string rootPath,
                                   std::filesystem::path indexFile)
    : index_(std::move(index)),
      rootPath_(std::move(rootPath)),
      indexFile_(std::move(indexFile)) {}

auto InteractiveShell::run() -> int {
  std::cout << "MiniSearch interactive shell. Type \"help\" for commands.\n";

  std::string line;
  while (true) {
    std::cout << "(minisearch) " << std::flush;
    if (!std::getline(std::cin, line)) {
      std::cout << '\n';
      return 0;
    }

    if (!execute(line)) {
      return 0;
    }
  }
}

auto InteractiveShell::execute(const std::string& line) -> bool {
  const std::pair<std::string, std::string> parsedCommand =
      splitCommand(trim(line));
  const std::string& command = parsedCommand.first;
  const std::string& argument = parsedCommand.second;
  if (command.empty()) {
    return true;
  }

  if (command == "quit" || command == "exit" || command == "q") {
    return false;
  }

  if (command == "help" || command == "h") {
    printHelp();
    return true;
  }

  if (command == "stats") {
    printStats();
    return true;
  }

  if (command == "path") {
    printPath();
    return true;
  }

  if (command == "show") {
    printShow();
    return true;
  }

  if (command == "find") {
    printFind(argument);
    return true;
  }

  if (command == "grep") {
    printGrep(argument);
    return true;
  }

  std::cout << "unknown command: " << command << '\n';
  return true;
}

auto InteractiveShell::printHelp() const -> void {
  std::cout << R"(Commands:
  find <query>   Search indexed file names and paths
  grep <query>   Search indexed text content
  show           Show all indexed files
  stats          Show index statistics
  path           Show current indexed root and index file
  help           Show this help
  quit           Exit the shell
)";
}

auto InteractiveShell::printStats() const -> void {
  std::cout << "files: " << index_.fileCount() << '\n'
            << "text files: " << index_.indexedTextFileCount() << '\n'
            << "terms: " << index_.termCount() << '\n';
}

auto InteractiveShell::printPath() const -> void {
  std::cout << "root: " << rootPath_ << '\n'
            << "index: " << indexFile_.string() << '\n';
}

auto InteractiveShell::printShow() const -> void {
  const std::vector<index::FileRecord>& records = index_.records();
  for (const auto& record : records) {
    std::cout << search::SearchEngine::formatRecord(record) << '\n';
  }
  std::cout << records.size() << " file(s)\n";
}

auto InteractiveShell::printFind(const std::string& query) const -> void {
  if (query.empty()) {
    std::cout << "usage: find <query>\n";
    return;
  }

  const search::SearchEngine engine(index_);
  printRecords(engine.findByName(query));
}

auto InteractiveShell::printGrep(const std::string& query) const -> void {
  if (query.empty()) {
    std::cout << "usage: grep <query>\n";
    return;
  }

  const search::SearchEngine engine(index_);
  printGrepMatches(engine.grepLines(query));
}

}  // namespace minisearch::shell
