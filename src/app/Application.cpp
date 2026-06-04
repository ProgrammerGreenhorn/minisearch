#include "minisearch/app/Application.hpp"

#include <iostream>
#include <utility>

#include "minisearch/cli/CommandParser.hpp"
#include "minisearch/index/IndexBuilder.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/shell/InteractiveShell.hpp"

namespace minisearch::app {

namespace {

using cli::Command;
using cli::CommandOptions;
using index::IndexBuilder;
using index::IndexRepository;
using index::IndexStorage;
using shell::InteractiveShell;

auto printIndexSummary(const index::InvertedIndex& search_index) -> void {
  std::cout << "files: " << search_index.fileCount() << '\n'
            << "text files: " << search_index.indexedTextFileCount() << '\n'
            << "terms: " << search_index.termCount() << '\n';
}

auto runIndex(const CommandOptions& command_options) -> int {
  IndexBuilder index_builder;
  IndexBuilder::Result build_result =
      index_builder.build({command_options.targetPath,
                           command_options.indexFile, command_options.threads});

  printIndexSummary(build_result.index);

  InteractiveShell interactive_shell(std::move(build_result.index),
                                     build_result.rootPath,
                                     build_result.indexFile);
  return interactive_shell.run();
}

auto runOpenCurrent() -> int {
  const IndexRepository::CurrentIndex current_index =
      IndexRepository::loadCurrentIndex();
  IndexStorage index_storage;
  index::InvertedIndex loaded_index =
      index_storage.load(current_index.indexFile);

  InteractiveShell interactive_shell(
      std::move(loaded_index), current_index.rootPath, current_index.indexFile);
  return interactive_shell.run();
}

}  // namespace

auto Application::run(int argument_count, char** argument_values) const -> int {
  cli::CommandParser command_parser;
  const CommandOptions command_options =
      command_parser.parse(argument_count, argument_values);

  switch (command_options.command) {
    case Command::OpenCurrent:
      return runOpenCurrent();
    case Command::Help:
      std::cout << cli::CommandParser::helpText();
      return 0;
    case Command::Index:
      return runIndex(command_options);
    case Command::Unknown:
      std::cerr << "unknown command\n" << cli::CommandParser::helpText();
      return 2;
  }

  return 0;
}

}  // namespace minisearch::app
