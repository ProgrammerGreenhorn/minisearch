#include <wx/app.h>

#include <exception>

#include "minisearch/config/AppConfig.hpp"
#include "minisearch/gui/MainFrame.hpp"
#include "minisearch/util/Logger.hpp"

namespace {

class MiniSearchGuiApp : public wxApp {
 public:
  auto OnInit() -> bool override {
    try {
      try {
        (void)minisearch::config::ensureDefaultConfigFile();
      } catch (const std::exception& exception) {
        MINISEARCH_LOG_WARNING(exception.what());
      }

      const minisearch::config::AppConfig app_config =
          minisearch::config::loadAppConfig();
      minisearch::config::configureLogger(app_config);
      auto* main_frame = new minisearch::gui::MainFrame(app_config);
      main_frame->Show(true);
      return true;
    } catch (const std::exception& exception) {
      MINISEARCH_LOG_ERROR(exception.what());
      return false;
    }
  }
};

}  // namespace

wxIMPLEMENT_APP(MiniSearchGuiApp);
