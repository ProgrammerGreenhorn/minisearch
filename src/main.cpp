#include <exception>

#include "minisearch/app/Application.hpp"
#include "minisearch/util/Logger.hpp"

auto main(int argc, char** argv) -> int {
  try {
    const minisearch::app::Application app;
    return app.run(argc, argv);
  } catch (const std::exception& error) {
    MINISEARCH_LOG_ERROR(error.what());
    return 1;
  }
}
