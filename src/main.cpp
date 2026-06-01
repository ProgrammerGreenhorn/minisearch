#include "minisearch/cli/CommandParser.hpp"
#include "minisearch/index/FileScanner.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/index/TextParser.hpp"
#include "minisearch/search/SearchEngine.hpp"
#include "minisearch/util/Logger.hpp"
#include "minisearch/util/ThreadPool.hpp"

#include <exception>
#include <future>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using minisearch::cli::Command;
using minisearch::cli::CommandOptions;
using minisearch::index::FileRecord;
using minisearch::index::FileScanner;
using minisearch::index::IndexStorage;
using minisearch::index::InvertedIndex;
using minisearch::index::TextParser;
using minisearch::search::SearchEngine;
using minisearch::util::Logger;
using minisearch::util::ThreadPool;

void printRecords(const std::vector<FileRecord>& records)
{
    for (const auto& record : records) {
        std::cout << SearchEngine::formatRecord(record) << '\n';
    }
    std::cout << records.size() << " result(s)\n";
}

int runIndex(const CommandOptions& options)
{
    Logger::info("scanning files...");
    FileScanner scanner;
    auto records = scanner.scan(options.targetPath);

    InvertedIndex index;
    for (auto& record : records) {
        index.addRecord(std::move(record));
    }

    Logger::info("parsing text files...");
    TextParser parser;
    ThreadPool pool(options.threads);

    using ParseResult = std::pair<InvertedIndex::DocumentId, std::vector<std::string>>;
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
    storage.save(options.indexFile, index);

    Logger::info("index written to " + options.indexFile.string());
    std::cout << "files: " << index.fileCount() << '\n'
              << "text files: " << index.indexedTextFileCount() << '\n'
              << "terms: " << index.termCount() << '\n';
    return 0;
}

int runFind(const CommandOptions& options)
{
    IndexStorage storage;
    const auto index = storage.load(options.indexFile);
    SearchEngine engine(index);
    printRecords(engine.findByName(options.query));
    return 0;
}

int runGrep(const CommandOptions& options)
{
    IndexStorage storage;
    const auto index = storage.load(options.indexFile);
    SearchEngine engine(index);
    printRecords(engine.grep(options.query));
    return 0;
}

int runStats(const CommandOptions& options)
{
    IndexStorage storage;
    const auto index = storage.load(options.indexFile);
    std::cout << "files: " << index.fileCount() << '\n'
              << "text files: " << index.indexedTextFileCount() << '\n'
              << "terms: " << index.termCount() << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        minisearch::cli::CommandParser parser;
        const auto options = parser.parse(argc, argv);

        switch (options.command) {
        case Command::Help:
            std::cout << minisearch::cli::CommandParser::helpText();
            return 0;
        case Command::Index:
            return runIndex(options);
        case Command::Find:
            return runFind(options);
        case Command::Grep:
            return runGrep(options);
        case Command::Stats:
            return runStats(options);
        case Command::Unknown:
            std::cerr << "unknown command\n"
                      << minisearch::cli::CommandParser::helpText();
            return 2;
        }
    } catch (const std::exception& error) {
        Logger::error(error.what());
        return 1;
    }

    return 0;
}

