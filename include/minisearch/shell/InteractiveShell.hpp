#pragma once

#include <filesystem>
#include <string>

#include "minisearch/index/InvertedIndex.hpp"

namespace minisearch::shell {

class InteractiveShell {
 public:
  InteractiveShell(index::InvertedIndex index, std::string rootPath,
                   std::filesystem::path indexFile);

  auto run() -> int;

 private:
  auto execute(const std::string& line) -> bool;

  auto printHelp() const -> void;

  auto printStats() const -> void;

  auto printPath() const -> void;

  auto printShow() const -> void;

  auto printFind(const std::string& query) const -> void;

  auto printGrep(const std::string& query) const -> void;

  index::InvertedIndex index_;
  std::string rootPath_;
  std::filesystem::path indexFile_;
};

}  // namespace minisearch::shell
