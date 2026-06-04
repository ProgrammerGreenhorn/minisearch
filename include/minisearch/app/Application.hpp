#pragma once

namespace minisearch::app {

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
