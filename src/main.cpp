#include <exception>

#include "minisearch/app/Application.hpp"
#include "minisearch/config/AppConfig.hpp"
#include "minisearch/util/Logger.hpp"

auto main(int argument_count, char** argument_values) -> int {
  try {
    try {
      (void)minisearch::config::ensureDefaultConfigFile();
    } catch (const std::exception& exception) {
      MINISEARCH_LOG_WARNING(exception.what());
    }

    const minisearch::config::AppConfig app_config =
        minisearch::config::loadAppConfig();
    minisearch::config::configureLogger(app_config);
    const minisearch::app::Application application(app_config);
    return application.run(argument_count, argument_values);
  } catch (const std::exception& exception) {
    MINISEARCH_LOG_ERROR(exception.what());
    return 1;
  }
}
