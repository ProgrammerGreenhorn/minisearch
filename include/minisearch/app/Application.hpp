#pragma once

namespace minisearch::app {

/**
 * @brief Coordinate startup and dispatch MiniSearch CLI actions.
 *
 * Application parses the command line, chooses the requested mode, and hands
 * off to the index builder or interactive shell.
 */
class Application {
 public:
  /**
   * @brief Run the application with command-line arguments.
   *
   * @param argument_count Number of command-line arguments.
   * @param argument_values Command-line argument vector.
   * @return Process exit code.
   */
  auto run(int argument_count, char** argument_values) const -> int;
};

}  // namespace minisearch::app
