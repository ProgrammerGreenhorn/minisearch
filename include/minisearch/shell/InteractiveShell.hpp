#pragma once

#include <filesystem>
#include <string>

#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::shell {

class InteractiveShell {
 public:
  /**
   * @brief Create a shell with no loaded index.
   */
  InteractiveShell();

  /**
   * @brief Create a shell for an already loaded index.
   *
   * @param search_index Search index used by shell commands.
   * @param root_path Canonical root path represented by the index.
   * @param index_file Protobuf index file loaded for this shell.
   */
  InteractiveShell(index::InvertedIndex search_index, std::string root_path,
                   std::filesystem::path index_file);

  /**
   * @brief Run the interactive shell loop.
   *
   * @return Process exit code for the shell session.
   */
  auto run() -> int;

 private:
  /**
   * @brief Execute one shell input line.
   *
   * @param input_line Raw line entered by the user.
   * @return True to keep the shell running; false to exit.
   */
  auto execute(const std::string& input_line) -> bool;

  /**
   * @brief Print shell command help.
   */
  auto printHelp() const -> void;

  /**
   * @brief Check whether an index is loaded before running a command.
   *
   * @param command_name Command that needs an index.
   * @return True when an index is loaded.
   */
  auto requireIndex(const std::string& command_name) const -> bool;

  /**
   * @brief Build an index for a path entered in the shell.
   *
   * @param path_text File or directory path to index.
   */
  auto indexPath(const std::string& path_text) -> void;

  /**
   * @brief Print recently built index roots.
   */
  auto printRecent() const -> void;

  /**
   * @brief Print index statistics.
   */
  auto printStats() const -> void;

  /**
   * @brief Print the current indexed root and index file path.
   */
  auto printPath() const -> void;

  /**
   * @brief Print all indexed file records.
   */
  auto printShow() const -> void;

  /**
   * @brief Print file-name search results.
   *
   * @param query_text Query matched against indexed paths and file names.
   */
  auto printFind(const std::string& query_text) const -> void;

  /**
   * @brief Print text-content grep results.
   *
   * @param query_text Text query matched against indexed file content.
   */
  auto printGrep(const std::string& query_text) const -> void;

  index::InvertedIndex index_;
  std::string rootPath_;
  std::filesystem::path indexFile_;
  bool hasIndex_ = false;
};

}  // namespace minisearch::shell
