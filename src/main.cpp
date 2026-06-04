#include <exception>

#include "minisearch/app/Application.hpp"
#include "minisearch/util/Logger.hpp"

auto main(int argument_count, char** argument_values) -> int {
  try {
    const minisearch::app::Application application;
    return application.run(argument_count, argument_values);
  } catch (const std::exception& exception) {
    MINISEARCH_LOG_ERROR(exception.what());
    return 1;
  }
}
