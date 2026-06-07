#pragma once

#include "minisearch/config/AppConfig.hpp"

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
   * @brief Create the application with runtime defaults.
   *
   * @param app_config Loaded application configuration.
   */
  explicit Application(config::AppConfig app_config);

  /**
   * @brief Run the application with command-line arguments.
   *
   * @param argument_count Number of command-line arguments.
   * @param argument_values Command-line argument vector.
   * @return Process exit code.
   */
  auto run(int argument_count, char** argument_values) const -> int;

 private:
  config::AppConfig appConfig_;
};

}  // namespace minisearch::app
