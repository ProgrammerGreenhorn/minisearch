#pragma once

#include <cstddef>
#include <filesystem>

#include "minisearch/index/FileScanner.hpp"

namespace minisearch::config {

enum class SearchMode { FileNames, GrepText };

/**
 * @brief Resolve the default worker-thread count for indexing.
 *
 * @return Hardware thread count, or a small fallback when unavailable.
 */
auto defaultThreadCount() -> std::size_t;

/**
 * @brief Runtime defaults loaded from the MiniSearch TOML config file.
 */
struct AppConfig {
  std::size_t threads = defaultThreadCount();
  index::FileScanner::Options scannerOptions;
  SearchMode defaultSearchMode = SearchMode::FileNames;
  bool openCurrentIndexOnStartup = true;
  std::filesystem::path logFile;
};

/**
 * @brief Resolve the default TOML config file path.
 *
 * @return Path to ~/.minisearch/config.toml.
 */
auto configFilePath() -> std::filesystem::path;

/**
 * @brief Create the default TOML config file when it is missing.
 *
 * Existing config files are left untouched.
 *
 * @return True when a new config file was created.
 */
auto ensureDefaultConfigFile() -> bool;

/**
 * @brief Create a default TOML config file at an explicit path when missing.
 *
 * Existing config files are left untouched.
 *
 * @param config_file TOML config file to create when absent.
 * @return True when a new config file was created.
 */
auto ensureDefaultConfigFile(const std::filesystem::path& config_file) -> bool;

/**
 * @brief Load the default MiniSearch TOML config.
 *
 * Missing config files are treated as default configuration.
 *
 * @return Loaded application configuration.
 */
auto loadAppConfig() -> AppConfig;

/**
 * @brief Load a MiniSearch TOML config from an explicit path.
 *
 * Missing config files are treated as default configuration.
 *
 * @param config_file TOML config file to load.
 * @return Loaded application configuration.
 */
auto loadAppConfigFromFile(const std::filesystem::path& config_file)
    -> AppConfig;

/**
 * @brief Configure the process logger from config and environment.
 *
 * MINISEARCH_LOG_FILE takes precedence over the TOML log_file setting.
 *
 * @param app_config Loaded application configuration.
 */
auto configureLogger(const AppConfig& app_config) -> void;

}  // namespace minisearch::config
