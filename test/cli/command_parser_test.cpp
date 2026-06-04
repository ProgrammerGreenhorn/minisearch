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

auto Parse(std::initializer_list<std::string> arguments) -> CommandOptions {
  std::vector<std::string> argument_storage(arguments);
  std::vector<char*> argument_values;
  argument_values.reserve(argument_storage.size());
  for (auto& argument_text : argument_storage) {
    argument_values.push_back(argument_text.data());
  }

  CommandParser command_parser;
  return command_parser.parse(static_cast<int>(argument_values.size()),
                              argument_values.data());
}

TEST(CommandParserTest, NoArgumentsOpenCurrentIndex) {
  const auto command_options = Parse({"minisearch"});

  EXPECT_EQ(command_options.command, Command::OpenCurrent);
  EXPECT_EQ(command_options.targetPath, std::filesystem::path("."));
  EXPECT_FALSE(command_options.targetPathExplicit);
  EXPECT_FALSE(command_options.indexFileExplicit);
  EXPECT_GT(command_options.threads, 0U);
}

TEST(CommandParserTest, HelpAliasesReturnHelpCommand) {
  EXPECT_EQ(Parse({"minisearch", "help"}).command, Command::Help);
  EXPECT_EQ(Parse({"minisearch", "--help"}).command, Command::Help);
  EXPECT_EQ(Parse({"minisearch", "-h"}).command, Command::Help);
}

TEST(CommandParserTest, LeadingDashCommandIsUnknown) {
  const auto command_options = Parse({"minisearch", "--unknown"});

  EXPECT_EQ(command_options.command, Command::Unknown);
  EXPECT_FALSE(command_options.targetPathExplicit);
}

TEST(CommandParserTest, ParsesIndexPathOutputAndThreads) {
  const auto command_options =
      Parse({"minisearch", "./src", "--output", "custom.pb", "--threads", "4"});

  EXPECT_EQ(command_options.command, Command::Index);
  EXPECT_EQ(command_options.targetPath, std::filesystem::path("./src"));
  EXPECT_TRUE(command_options.targetPathExplicit);
  EXPECT_EQ(command_options.indexFile, std::filesystem::path("custom.pb"));
  EXPECT_TRUE(command_options.indexFileExplicit);
  EXPECT_EQ(command_options.threads, 4U);
}

TEST(CommandParserTest, ParsesShortOutputOption) {
  const auto command_options = Parse({"minisearch", ".", "-o", "index.pb"});

  EXPECT_EQ(command_options.command, Command::Index);
  EXPECT_EQ(command_options.indexFile, std::filesystem::path("index.pb"));
  EXPECT_TRUE(command_options.indexFileExplicit);
}

TEST(CommandParserTest, ThrowsForUnknownIndexOption) {
  EXPECT_THROW(Parse({"minisearch", ".", "--bad-option"}),
               std::invalid_argument);
}

TEST(CommandParserTest, ThrowsForMissingOptionValue) {
  EXPECT_THROW(Parse({"minisearch", ".", "--output"}), std::invalid_argument);
}

}  // namespace
