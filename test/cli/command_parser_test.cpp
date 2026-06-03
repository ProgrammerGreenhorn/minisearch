#include <gtest/gtest.h>

#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "minisearch/cli/CommandParser.hpp"

namespace {

using minisearch::cli::Command;
using minisearch::cli::CommandOptions;
using minisearch::cli::CommandParser;

auto Parse(std::initializer_list<std::string> args) -> CommandOptions {
  std::vector<std::string> storage(args);
  std::vector<char*> argv;
  argv.reserve(storage.size());
  for (auto& arg : storage) {
    argv.push_back(arg.data());
  }

  CommandParser parser;
  return parser.parse(static_cast<int>(argv.size()), argv.data());
}

TEST(CommandParserTest, NoArgumentsOpenCurrentIndex) {
  const auto options = Parse({"minisearch"});

  EXPECT_EQ(options.command, Command::OpenCurrent);
  EXPECT_EQ(options.targetPath, std::filesystem::path("."));
  EXPECT_FALSE(options.targetPathExplicit);
  EXPECT_FALSE(options.indexFileExplicit);
  EXPECT_GT(options.threads, 0U);
}

TEST(CommandParserTest, HelpAliasesReturnHelpCommand) {
  EXPECT_EQ(Parse({"minisearch", "help"}).command, Command::Help);
  EXPECT_EQ(Parse({"minisearch", "--help"}).command, Command::Help);
  EXPECT_EQ(Parse({"minisearch", "-h"}).command, Command::Help);
}

TEST(CommandParserTest, LeadingDashCommandIsUnknown) {
  const auto options = Parse({"minisearch", "--unknown"});

  EXPECT_EQ(options.command, Command::Unknown);
  EXPECT_FALSE(options.targetPathExplicit);
}

TEST(CommandParserTest, ParsesIndexPathOutputAndThreads) {
  const auto options =
      Parse({"minisearch", "./src", "--output", "custom.pb", "--threads", "4"});

  EXPECT_EQ(options.command, Command::Index);
  EXPECT_EQ(options.targetPath, std::filesystem::path("./src"));
  EXPECT_TRUE(options.targetPathExplicit);
  EXPECT_EQ(options.indexFile, std::filesystem::path("custom.pb"));
  EXPECT_TRUE(options.indexFileExplicit);
  EXPECT_EQ(options.threads, 4U);
}

TEST(CommandParserTest, ParsesShortOutputOption) {
  const auto options = Parse({"minisearch", ".", "-o", "index.pb"});

  EXPECT_EQ(options.command, Command::Index);
  EXPECT_EQ(options.indexFile, std::filesystem::path("index.pb"));
  EXPECT_TRUE(options.indexFileExplicit);
}

TEST(CommandParserTest, ThrowsForUnknownIndexOption) {
  EXPECT_THROW(Parse({"minisearch", ".", "--bad-option"}),
               std::invalid_argument);
}

TEST(CommandParserTest, ThrowsForMissingOptionValue) {
  EXPECT_THROW(Parse({"minisearch", ".", "--output"}), std::invalid_argument);
}

}  // namespace
