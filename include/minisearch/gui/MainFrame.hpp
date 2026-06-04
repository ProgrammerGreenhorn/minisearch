#pragma once

#include <wx/frame.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "minisearch/index/InvertedIndex.hpp"

class wxButton;
class wxChoice;
class wxListCtrl;
class wxTextCtrl;

namespace minisearch::gui {

class MainFrame : public wxFrame {
 public:
  /**
   * @brief Create the MiniSearch main window.
   */
  MainFrame();

 private:
  struct ResultEntry {
    std::filesystem::path path;
    std::uint32_t line = 0;
  };

  /**
   * @brief Load the current index pointer and index file if available.
   */
  auto loadCurrentIndex() -> void;

  /**
   * @brief Build or refresh an index for the path in the path control.
   */
  auto indexPath() -> void;

  /**
   * @brief Run the active search mode using the query control value.
   */
  auto runSearch() -> void;

  /**
   * @brief Show indexed files when there is no active query.
   *
   * @param limit Maximum number of files to show.
   */
  auto showIndexedFiles(std::size_t limit) -> void;

  /**
   * @brief Show the no-index placeholder row.
   */
  auto showNoIndexState() -> void;

  /**
   * @brief Update status text and enabled states from the current index state.
   */
  auto updateIndexState() -> void;

  /**
   * @brief Resize result-list columns to match the current window width.
   */
  auto updateResultColumns() -> void;

  /**
   * @brief Remove all rows from the result list.
   */
  auto clearResults() -> void;

  /**
   * @brief Show an error dialog.
   *
   * @param title Dialog title.
   * @param message Error message body.
   */
  auto showError(const std::string& title, const std::string& message) -> void;

  /**
   * @brief Handle directory selection.
   */
  auto onBrowseDirectory() -> void;

  /**
   * @brief Handle file selection.
   */
  auto onBrowseFile() -> void;

  /**
   * @brief Handle the index button.
   */
  auto onIndex() -> void;

  /**
   * @brief Handle the search button or Enter key in the query field.
   */
  auto onSearch() -> void;

  /**
   * @brief Open the file represented by an activated result row.
   *
   * @param row_index Result row index in the list control.
   */
  auto onResultActivated(long row_index) -> void;

  wxTextCtrl* pathCtrl_ = nullptr;
  wxButton* browseDirectoryButton_ = nullptr;
  wxButton* browseFileButton_ = nullptr;
  wxButton* indexButton_ = nullptr;
  wxChoice* searchModeChoice_ = nullptr;
  wxTextCtrl* queryCtrl_ = nullptr;
  wxButton* searchButton_ = nullptr;
  wxListCtrl* resultsList_ = nullptr;

  index::InvertedIndex index_;
  std::string rootPath_;
  std::filesystem::path indexFile_;
  bool hasIndex_ = false;
  std::vector<ResultEntry> resultEntries_;
};

}  // namespace minisearch::gui
