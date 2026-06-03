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

auto printIndexSummary(const index::InvertedIndex& index) -> void {
  std::cout << "files: " << index.fileCount() << '\n'
            << "text files: " << index.indexedTextFileCount() << '\n'
            << "terms: " << index.termCount() << '\n';
}

auto runIndex(const CommandOptions& options) -> int {
  IndexBuilder builder;
  IndexBuilder::Result result =
      builder.build({options.targetPath, options.indexFile, options.threads});

  printIndexSummary(result.index);

  InteractiveShell shell(std::move(result.index), result.rootPath,
                         result.indexFile);
  return shell.run();
}

auto runOpenCurrent() -> int {
  const IndexRepository::CurrentIndex current =
      IndexRepository::loadCurrentIndex();
  IndexStorage storage;
  index::InvertedIndex index = storage.load(current.indexFile);

  InteractiveShell shell(std::move(index), current.rootPath, current.indexFile);
  return shell.run();
}

}  // namespace

auto Application::run(int argc, char** argv) const -> int {
  cli::CommandParser parser;
  const CommandOptions options = parser.parse(argc, argv);

  switch (options.command) {
    case Command::OpenCurrent:
      return runOpenCurrent();
    case Command::Help:
      std::cout << cli::CommandParser::helpText();
      return 0;
    case Command::Index:
      return runIndex(options);
    case Command::Unknown:
      std::cerr << "unknown command\n" << cli::CommandParser::helpText();
      return 2;
  }

  return 0;
}

}  // namespace minisearch::app
