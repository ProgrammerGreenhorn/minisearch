#include "minisearch/shell/InteractiveShell.hpp"

#include <cctype>
#include <exception>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include "minisearch/index/IndexBuilder.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/search/SearchEngine.hpp"

namespace minisearch::shell {

namespace {

auto trim(std::string input_text) -> std::string {
  auto first_non_space = input_text.begin();
  while (first_non_space != input_text.end() &&
         std::isspace(static_cast<unsigned char>(*first_non_space))) {
    ++first_non_space;
  }

  auto last_non_space = input_text.end();
  while (last_non_space != first_non_space &&
         std::isspace(static_cast<unsigned char>(*(last_non_space - 1)))) {
    --last_non_space;
  }

  return std::string(first_non_space, last_non_space);
}

auto splitCommand(const std::string& input_line)
    -> std::pair<std::string, std::string> {
  std::istringstream input_stream(input_line);
  std::string command_name;
  input_stream >> command_name;

  std::string command_argument;
  std::getline(input_stream, command_argument);
  return {command_name, trim(command_argument)};
}

auto printRecords(const std::vector<index::FileRecord>& file_records) -> void {
  for (const auto& file_record : file_records) {
    std::cout << search::SearchEngine::formatRecord(file_record) << '\n';
  }
  std::cout << file_records.size() << " result(s)\n";
}

auto printGrepMatches(
    const std::vector<search::SearchEngine::GrepLine>& grep_lines) -> void {
  for (std::size_t match_index = 0; match_index < grep_lines.size();
       ++match_index) {
    std::cout << search::SearchEngine::formatGrepLine(grep_lines[match_index])
              << '\n';
    if (match_index + 1 < grep_lines.size()) {
      std::cout << '\n';
    }
  }
  std::cout << grep_lines.size() << " line(s)\n";
}

}  // namespace

InteractiveShell::InteractiveShell(config::AppConfig app_config)
    : appConfig_(std::move(app_config)) {}

InteractiveShell::InteractiveShell(index::InvertedIndex search_index,
                                   std::string root_path,
                                   std::filesystem::path index_file,
                                   config::AppConfig app_config)
    : index_(std::move(search_index)),
      rootPath_(std::move(root_path)),
      indexFile_(std::move(index_file)),
      hasIndex_(true),
      appConfig_(std::move(app_config)) {}

auto InteractiveShell::run() -> int {
  std::cout << "MiniSearch interactive shell. Type \"help\" for commands.\n";
  if (!hasIndex_) {
    std::cout << "No index loaded. Use \"index <path>\" to build one.\n";
  }

  std::string input_line;
  while (true) {
    std::cout << "(minisearch) " << std::flush;
    if (!std::getline(std::cin, input_line)) {
      std::cout << '\n';
      return 0;
    }

    if (!execute(input_line)) {
      return 0;
    }
  }
}

auto InteractiveShell::execute(const std::string& input_line) -> bool {
  const std::pair<std::string, std::string> parsed_command =
      splitCommand(trim(input_line));
  const std::string& command_name = parsed_command.first;
  const std::string& command_argument = parsed_command.second;
  if (command_name.empty()) {
    return true;
  }

  if (command_name == "quit" || command_name == "exit" || command_name == "q") {
    return false;
  }

  if (command_name == "help" || command_name == "h") {
    printHelp();
    return true;
  }

  if (command_name == "index" || command_name == "parse") {
    indexPath(command_argument);
    return true;
  }

  if (command_name == "recent") {
    printRecent();
    return true;
  }

  if (command_name == "stats") {
    if (requireIndex(command_name)) {
      printStats();
    }
    return true;
  }

  if (command_name == "path") {
    if (requireIndex(command_name)) {
      printPath();
    }
    return true;
  }

  if (command_name == "show") {
    if (requireIndex(command_name)) {
      printShow();
    }
    return true;
  }

  if (command_name == "find") {
    if (requireIndex(command_name)) {
      printFind(command_argument);
    }
    return true;
  }

  if (command_name == "grep") {
    if (requireIndex(command_name)) {
      printGrep(command_argument);
    }
    return true;
  }

  std::cout << "unknown command: " << command_name << '\n';
  return true;
}

auto InteractiveShell::printHelp() const -> void {
  std::cout << R"(Commands:
  index <path>   Build or refresh an index for a file or directory
  parse <path>   Alias for index <path>
  recent         Show recently indexed roots
  find <query>   Search indexed file names and paths
  grep <query>   Search indexed text content
  show           Show all indexed files
  stats          Show index statistics
  path           Show current indexed root and index file
  help           Show this help
  quit           Exit the shell
)";
}

auto InteractiveShell::requireIndex(const std::string& command_name) const
    -> bool {
  if (hasIndex_) {
    return true;
  }

  std::cout << command_name
            << " requires a loaded index; use index <path> first.\n";
  return false;
}

auto InteractiveShell::indexPath(const std::string& path_text) -> void {
  if (path_text.empty()) {
    std::cout << "usage: index <path>\n";
    return;
  }

  try {
    const std::filesystem::path target_path(path_text);
    const std::filesystem::path index_file =
        index::IndexRepository::indexFileForPath(target_path);
    index::IndexBuilder index_builder;
    index::IndexBuilder::Result build_result =
        index_builder.build({target_path, index_file, appConfig_.threads,
                             appConfig_.scannerOptions});

    index_ = std::move(build_result.index);
    rootPath_ = std::move(build_result.rootPath);
    indexFile_ = std::move(build_result.indexFile);
    hasIndex_ = true;

    std::cout << "indexed: " << rootPath_ << '\n'
              << "reused text files: " << build_result.reusedTextFiles << '\n'
              << "parsed text files: " << build_result.parsedTextFiles << '\n';
    printStats();
  } catch (const std::exception& exception) {
    std::cout << "index failed: " << exception.what() << '\n';
  }
}

auto InteractiveShell::printRecent() const -> void {
  try {
    const std::vector<index::IndexRepository::ManagedIndex> recent_indexes =
        index::IndexRepository::loadRecentIndexes();
    if (recent_indexes.empty()) {
      std::cout << "no recent indexes\n";
      return;
    }

    for (std::size_t recent_index = 0; recent_index < recent_indexes.size();
         ++recent_index) {
      std::cout << recent_index + 1 << ". "
                << recent_indexes[recent_index].rootPath << '\n';
    }
  } catch (const std::exception& exception) {
    std::cout << "failed to load recent indexes: " << exception.what() << '\n';
  }
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
  const std::vector<index::FileRecord>& file_records = index_.records();
  for (const auto& file_record : file_records) {
    std::cout << search::SearchEngine::formatRecord(file_record) << '\n';
  }
  std::cout << file_records.size() << " file(s)\n";
}

auto InteractiveShell::printFind(const std::string& query_text) const -> void {
  if (query_text.empty()) {
    std::cout << "usage: find <query>\n";
    return;
  }

  const search::SearchEngine search_engine(index_);
  printRecords(search_engine.findByName(query_text));
}

auto InteractiveShell::printGrep(const std::string& query_text) const -> void {
  if (query_text.empty()) {
    std::cout << "usage: grep <query>\n";
    return;
  }

  const search::SearchEngine search_engine(index_);
  printGrepMatches(search_engine.grepLines(query_text));
}

}  // namespace minisearch::shell
