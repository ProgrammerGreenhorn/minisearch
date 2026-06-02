#include <exception>
#include <future>
#include <iostream>
#include <utility>
#include <vector>

#include "minisearch/cli/CommandParser.hpp"
#include "minisearch/index/FileScanner.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/index/TextParser.hpp"
#include "minisearch/shell/InteractiveShell.hpp"
#include "minisearch/util/Logger.hpp"
#include "minisearch/util/ThreadPool.hpp"

namespace {

using minisearch::cli::Command;
using minisearch::cli::CommandOptions;
using minisearch::index::FileScanner;
using minisearch::index::IndexRepository;
using minisearch::index::IndexStorage;
using minisearch::index::InvertedIndex;
using minisearch::index::TextParser;
using minisearch::shell::InteractiveShell;
using minisearch::util::ThreadPool;

auto runIndex(const CommandOptions& options) -> int {
  MINISEARCH_LOG_INFO("scanning files...");
  FileScanner scanner;
  auto records = scanner.scan(options.targetPath);

  InvertedIndex index;
  for (auto& record : records) {
    index.addRecord(std::move(record));
  }

  MINISEARCH_LOG_INFO("parsing text files...");
  TextParser parser;
  ThreadPool pool(options.threads);

  // the map from document id to the list of terms in the document
  using ParseResult =
      std::pair<InvertedIndex::DocumentId, std::vector<std::string>>;
  std::vector<std::future<ParseResult>> futures;

  for (const auto& record : index.records()) {
    if (!record.textIndexed) {
      continue;
    }

    futures.push_back(pool.submit([record, &parser]() -> ParseResult {
      return {record.id, parser.parseFile(record.path)};
    }));
  }

  for (auto& future : futures) {
    auto [id, terms] = future.get();
    index.addTerms(id, terms);
  }

  IndexStorage storage;
  const auto rootPath = IndexRepository::canonicalKey(options.targetPath);
  storage.save(options.indexFile, index, rootPath);
  IndexRepository::saveCurrentIndex(rootPath, options.indexFile);

  MINISEARCH_LOG_INFO("index written to " + options.indexFile.string());
  MINISEARCH_LOG_INFO("current index root set to " + rootPath);
  std::cout << "files: " << index.fileCount() << '\n'
            << "text files: " << index.indexedTextFileCount() << '\n'
            << "terms: " << index.termCount() << '\n';

  InteractiveShell shell(std::move(index), rootPath, options.indexFile);
  return shell.run();
}

auto runOpenCurrent() -> int {
  const auto current = IndexRepository::loadCurrentIndex();
  IndexStorage storage;
  auto index = storage.load(current.indexFile);
  InteractiveShell shell(std::move(index), current.rootPath, current.indexFile);
  return shell.run();
}

}  // namespace

auto main(int argc, char** argv) -> int {
  try {
    minisearch::cli::CommandParser parser;
    const auto options = parser.parse(argc, argv);

    switch (options.command) {
      case Command::OpenCurrent:
        return runOpenCurrent();
      case Command::Help:
        std::cout << minisearch::cli::CommandParser::helpText();
        return 0;
      case Command::Index:
        return runIndex(options);
      case Command::Unknown:
        std::cerr << "unknown command\n"
                  << minisearch::cli::CommandParser::helpText();
        return 2;
    }
  } catch (const std::exception& error) {
    MINISEARCH_LOG_ERROR(error.what());
    return 1;
  }

  return 0;
}
