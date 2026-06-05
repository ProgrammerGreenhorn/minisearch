#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/shell/InteractiveShell.hpp"
#include "minisearch/util/Logger.hpp"

namespace {

using minisearch::index::IndexRepository;
using minisearch::shell::InteractiveShell;

class ScopedTempDir {
 public:
  explicit ScopedTempDir(const std::string& name) {
    const auto unique_suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (name + "_" + std::to_string(unique_suffix));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ignored_error;
    std::filesystem::remove_all(path_, ignored_error);
  }

  auto path() const -> const std::filesystem::path& { return path_; }

 private:
  std::filesystem::path path_;
};

class ScopedHome {
 public:
  explicit ScopedHome(const std::filesystem::path& home_path) {
    const char* previous_home_value = std::getenv("HOME");
    if (previous_home_value != nullptr) {
      old_home_ = previous_home_value;
      had_old_home_ = true;
    }
    setenv("HOME", home_path.string().c_str(), 1);
  }

  ~ScopedHome() {
    if (had_old_home_) {
      setenv("HOME", old_home_.c_str(), 1);
    } else {
      unsetenv("HOME");
    }
  }

 private:
  std::string old_home_;
  bool had_old_home_ = false;
};

class ScopedIoRedirect {
 public:
  explicit ScopedIoRedirect(const std::string& input_text)
      : input_stream_(input_text),
        old_input_(std::cin.rdbuf(input_stream_.rdbuf())),
        old_output_(std::cout.rdbuf(output_stream_.rdbuf())) {}

  ~ScopedIoRedirect() {
    std::cin.rdbuf(old_input_);
    std::cout.rdbuf(old_output_);
  }

  auto output() const -> std::string { return output_stream_.str(); }

 private:
  std::istringstream input_stream_;
  std::ostringstream output_stream_;
  std::streambuf* old_input_;
  std::streambuf* old_output_;
};

TEST(InteractiveShellTest, EmptyShellReportsMissingIndexForSearchCommands) {
  ScopedIoRedirect io_redirect("stats\nquit\n");
  InteractiveShell interactive_shell;

  EXPECT_EQ(interactive_shell.run(), 0);

  const std::string output_text = io_redirect.output();
  EXPECT_NE(output_text.find("No index loaded"), std::string::npos);
  EXPECT_NE(output_text.find("stats requires a loaded index"),
            std::string::npos);
}

TEST(InteractiveShellTest, RecentCommandPrintsRecentIndexRoots) {
  ScopedTempDir temp_dir("minisearch_shell_recent");
  ScopedHome scoped_home(temp_dir.path());
  const auto indexed_root = temp_dir.path() / "project";
  const auto index_file = temp_dir.path() / "index.pb";
  IndexRepository::saveCurrentIndex(indexed_root.string(), index_file);
  minisearch::util::Logger::instance().flush();

  ScopedIoRedirect io_redirect("recent\nquit\n");
  InteractiveShell interactive_shell;

  EXPECT_EQ(interactive_shell.run(), 0);

  const std::string output_text = io_redirect.output();
  EXPECT_NE(output_text.find(indexed_root.string()), std::string::npos);
}

}  // namespace
