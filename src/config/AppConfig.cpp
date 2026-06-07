#include "minisearch/config/AppConfig.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <toml++/toml.hpp>
#include <vector>

#include "minisearch/util/Logger.hpp"

namespace minisearch::config {

namespace {

constexpr std::string_view ConfigDirectoryName = ".minisearch";
constexpr std::string_view ConfigFileName = "config.toml";
constexpr std::string_view LogFileEnvironmentVariable = "MINISEARCH_LOG_FILE";

auto stringArrayOr(const toml::table& config_table, std::string_view key,
                   const std::vector<std::string>& fallback)
    -> std::vector<std::string> {
  const toml::array* array_value = config_table[key].as_array();
  if (array_value == nullptr) {
    return fallback;
  }

  std::vector<std::string> values;
  values.reserve(array_value->size());
  for (const toml::node& item_value : *array_value) {
    const std::optional<std::string> item_text =
        item_value.value<std::string>();
    if (item_text.has_value()) {
      values.push_back(*item_text);
    }
  }
  return values;
}

auto parseSearchMode(const std::string& mode_text) -> SearchMode {
  if (mode_text == "file_names" || mode_text == "files" ||
      mode_text == "name") {
    return SearchMode::FileNames;
  }
  if (mode_text == "grep_text" || mode_text == "grep" ||
      mode_text == "content") {
    return SearchMode::GrepText;
  }

  throw std::invalid_argument("unknown default_search_mode: " + mode_text);
}

auto applyIndexConfig(AppConfig& app_config,
                      const toml::table& config_table) -> void {
  if (const auto thread_count =
          config_table["threads"].value<std::uint64_t>()) {
    app_config.threads = *thread_count == 0
                             ? defaultThreadCount()
                             : static_cast<std::size_t>(*thread_count);
  }

  if (const auto max_text_file_bytes =
          config_table["max_text_file_bytes"].value<std::uint64_t>()) {
    app_config.scannerOptions.maxTextFileBytes = *max_text_file_bytes;
  }

  app_config.scannerOptions.excludedNames = stringArrayOr(
      config_table, "excluded_names", app_config.scannerOptions.excludedNames);
}

auto applySearchConfig(AppConfig& app_config,
                       const toml::table& config_table) -> void {
  if (const auto mode_text =
          config_table["default_mode"].value<std::string>()) {
    app_config.defaultSearchMode = parseSearchMode(*mode_text);
  }
}

auto applyStartupConfig(AppConfig& app_config,
                        const toml::table& config_table) -> void {
  app_config.openCurrentIndexOnStartup =
      config_table["open_current_index"].value_or(
          app_config.openCurrentIndexOnStartup);
}

auto applyLoggingConfig(AppConfig& app_config,
                        const toml::table& config_table) -> void {
  if (const auto log_file = config_table["file"].value<std::string>()) {
    app_config.logFile = *log_file;
  }
}

auto applyConfigTable(AppConfig& app_config,
                      const toml::table& config_table) -> void {
  if (const toml::table* index_table = config_table["index"].as_table()) {
    applyIndexConfig(app_config, *index_table);
  }

  if (const toml::table* search_table = config_table["search"].as_table()) {
    applySearchConfig(app_config, *search_table);
  }

  if (const toml::table* startup_table = config_table["startup"].as_table()) {
    applyStartupConfig(app_config, *startup_table);
  }

  if (const toml::table* logging_table = config_table["logging"].as_table()) {
    applyLoggingConfig(app_config, *logging_table);
  }
}

}  // namespace

auto defaultThreadCount() -> std::size_t {
  const unsigned int hardware_thread_count =
      std::thread::hardware_concurrency();
  return hardware_thread_count == 0 ? 2 : hardware_thread_count;
}

auto configFilePath() -> std::filesystem::path {
  const char* home_value = std::getenv("HOME");
  if (home_value == nullptr || std::string_view(home_value).empty()) {
    throw std::runtime_error("HOME is not set; cannot resolve ~/.minisearch");
  }

  return std::filesystem::path(home_value) / ConfigDirectoryName /
         ConfigFileName;
}

auto loadAppConfig() -> AppConfig {
  return loadAppConfigFromFile(configFilePath());
}

auto loadAppConfigFromFile(const std::filesystem::path& config_file)
    -> AppConfig {
  std::error_code exists_error;
  const bool config_exists = std::filesystem::exists(config_file, exists_error);
  if (exists_error) {
    throw std::runtime_error("failed to check config file: " +
                             config_file.string());
  }
  if (!config_exists) {
    return AppConfig{};
  }

  try {
    AppConfig app_config;
    const toml::table config_table = toml::parse_file(config_file.string());
    applyConfigTable(app_config, config_table);
    return app_config;
  } catch (const toml::parse_error& exception) {
    throw std::runtime_error("failed to parse config file " +
                             config_file.string() + ": " + exception.what());
  } catch (const std::exception& exception) {
    throw std::runtime_error("failed to load config file " +
                             config_file.string() + ": " + exception.what());
  }
}

auto configureLogger(const AppConfig& app_config) -> void {
  if (app_config.logFile.empty()) {
    util::Logger::instance().clearLogFile();
  } else {
    util::Logger::instance().setLogFile(app_config.logFile);
  }

  const char* log_file_value = std::getenv(LogFileEnvironmentVariable.data());
  if (log_file_value == nullptr) {
    return;
  }

  if (std::string_view(log_file_value).empty()) {
    util::Logger::instance().clearLogFile();
    return;
  }

  util::Logger::instance().setLogFile(log_file_value);
}

}  // namespace minisearch::config
