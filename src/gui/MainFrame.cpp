#include "minisearch/gui/MainFrame.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/window.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "minisearch/index/IndexBuilder.hpp"
#include "minisearch/index/IndexRepository.hpp"
#include "minisearch/index/IndexStorage.hpp"
#include "minisearch/search/SearchEngine.hpp"

namespace minisearch::gui {

namespace {

constexpr long SearchLimit = 200;
constexpr int MinimumWindowWidth = 900;
constexpr int MinimumWindowHeight = 560;
constexpr int ListChromeWidth = 28;
constexpr int LineColumnWidth = 84;

auto pageBackgroundColor() -> wxColour { return wxColour(245, 247, 251); }

auto inputBackgroundColor() -> wxColour { return wxColour(255, 255, 255); }

auto primaryColor() -> wxColour { return wxColour(37, 99, 235); }

auto accentColor() -> wxColour { return wxColour(15, 118, 110); }

auto softButtonColor() -> wxColour { return wxColour(226, 232, 240); }

auto textColor() -> wxColour { return wxColour(17, 24, 39); }

auto mutedTextColor() -> wxColour { return wxColour(71, 85, 105); }

auto resultRowColor(long row_index) -> wxColour {
  return row_index % 2 == 0 ? wxColour(255, 255, 255) : wxColour(238, 246, 255);
}

auto defaultThreadCount() -> std::size_t {
  const unsigned int hardware_thread_count =
      std::thread::hardware_concurrency();
  return hardware_thread_count == 0 ? 2 : hardware_thread_count;
}

auto trim(std::string input_text) -> std::string {
  auto first_non_space = input_text.begin();
  while (first_non_space != input_text.end() &&
         std::isspace(static_cast<unsigned char>(*first_non_space))) {
    ++first_non_space;
  }

  auto last_non_space = input_text.end();
  while (last_non_space != first_non_space &&
         std::isspace(static_cast<unsigned char>(*(last_non_space - 1)))) {
    --last_non_space;
  }

  return std::string(first_non_space, last_non_space);
}

auto fromWxString(const wxString& wx_value) -> std::string {
  const wxCharBuffer utf8_buffer = wx_value.ToUTF8();
  const char* utf8_data = utf8_buffer.data();
  return utf8_data == nullptr ? std::string{} : std::string(utf8_data);
}

auto toWxString(const std::string& input_text) -> wxString {
  return wxString::FromUTF8(input_text.c_str());
}

auto toWxString(const std::filesystem::path& file_path) -> wxString {
  return toWxString(file_path.string());
}

auto sizeText(std::uintmax_t file_size) -> std::string {
  return std::to_string(file_size) + " bytes";
}

auto indexSummary(const index::InvertedIndex& search_index) -> std::string {
  std::ostringstream summary_stream;
  summary_stream << "Files: " << search_index.fileCount()
                 << "  Text files: " << search_index.indexedTextFileCount()
                 << "  Terms: " << search_index.termCount();
  return summary_stream.str();
}

auto styleLabel(wxStaticText* label) -> void {
  label->SetForegroundColour(mutedTextColor());
}

auto styleTextControl(wxTextCtrl* text_control) -> void {
  text_control->SetBackgroundColour(inputBackgroundColor());
  text_control->SetForegroundColour(textColor());
}

auto styleChoice(wxChoice* choice) -> void {
  choice->SetBackgroundColour(inputBackgroundColor());
  choice->SetForegroundColour(textColor());
}

auto styleButton(wxButton* button, const wxColour& background_color,
                 const wxColour& foreground_color) -> void {
  button->SetBackgroundColour(background_color);
  button->SetForegroundColour(foreground_color);
}

auto styleResultsList(wxListCtrl* results_list) -> void {
  results_list->SetBackgroundColour(inputBackgroundColor());
  results_list->SetForegroundColour(textColor());
}

auto styleResultRow(wxListCtrl* results_list, long row_index) -> void {
  results_list->SetItemBackgroundColour(row_index, resultRowColor(row_index));
  results_list->SetItemTextColour(row_index, textColor());
}

auto styleStatusBar(wxStatusBar* status_bar) -> void {
  if (status_bar == nullptr) {
    return;
  }
  status_bar->SetBackgroundColour(accentColor());
  status_bar->SetForegroundColour(wxColour(240, 253, 250));
}

}  // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "MiniSearch", wxDefaultPosition,
              wxSize(1280, 820)) {
  auto* main_panel = new wxPanel(this);
  SetMinSize(wxSize(MinimumWindowWidth, MinimumWindowHeight));
  SetBackgroundColour(pageBackgroundColor());
  main_panel->SetBackgroundColour(pageBackgroundColor());

  auto* root_sizer = new wxBoxSizer(wxVERTICAL);

  auto* path_sizer = new wxBoxSizer(wxHORIZONTAL);
  auto* path_label = new wxStaticText(main_panel, wxID_ANY, "Path");
  styleLabel(path_label);
  path_sizer->Add(path_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

  pathCtrl_ = new wxTextCtrl(main_panel, wxID_ANY);
  pathCtrl_->SetMinSize(wxSize(260, -1));
  styleTextControl(pathCtrl_);
  path_sizer->Add(pathCtrl_, 1, wxRIGHT, 8);

  browseDirectoryButton_ = new wxButton(main_panel, wxID_ANY, "Folder");
  browseDirectoryButton_->SetMinSize(wxSize(86, -1));
  styleButton(browseDirectoryButton_, softButtonColor(), textColor());
  path_sizer->Add(browseDirectoryButton_, 0, wxRIGHT, 6);

  browseFileButton_ = new wxButton(main_panel, wxID_ANY, "File");
  browseFileButton_->SetMinSize(wxSize(70, -1));
  styleButton(browseFileButton_, softButtonColor(), textColor());
  path_sizer->Add(browseFileButton_, 0, wxRIGHT, 6);

  indexButton_ = new wxButton(main_panel, wxID_ANY, "Index");
  indexButton_->SetMinSize(wxSize(78, -1));
  styleButton(indexButton_, accentColor(), wxColour(240, 253, 250));
  path_sizer->Add(indexButton_, 0);
  root_sizer->Add(path_sizer, 0, wxEXPAND | wxALL, 12);

  auto* search_sizer = new wxBoxSizer(wxHORIZONTAL);
  searchModeChoice_ = new wxChoice(main_panel, wxID_ANY);
  searchModeChoice_->SetMinSize(wxSize(150, -1));
  styleChoice(searchModeChoice_);
  searchModeChoice_->Append("File names");
  searchModeChoice_->Append("Text content");
  searchModeChoice_->SetSelection(0);
  search_sizer->Add(searchModeChoice_, 0, wxRIGHT, 8);

  queryCtrl_ =
      new wxTextCtrl(main_panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDefaultSize, wxTE_PROCESS_ENTER);
  queryCtrl_->SetMinSize(wxSize(260, -1));
  styleTextControl(queryCtrl_);
  search_sizer->Add(queryCtrl_, 1, wxRIGHT, 8);

  searchButton_ = new wxButton(main_panel, wxID_ANY, "Search");
  searchButton_->SetMinSize(wxSize(90, -1));
  styleButton(searchButton_, primaryColor(), wxColour(239, 246, 255));
  search_sizer->Add(searchButton_, 0);
  root_sizer->Add(search_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  resultsList_ = new wxListCtrl(main_panel, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  styleResultsList(resultsList_);
  resultsList_->InsertColumn(0, "Path", wxLIST_FORMAT_LEFT, 520);
  resultsList_->InsertColumn(1, "Line", wxLIST_FORMAT_LEFT, 80);
  resultsList_->InsertColumn(2, "Details", wxLIST_FORMAT_LEFT, 460);
  root_sizer->Add(resultsList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  main_panel->SetSizer(root_sizer);
  CreateStatusBar(2);
  const int status_widths[] = {-3, -2};
  SetStatusWidths(2, status_widths);
  styleStatusBar(GetStatusBar());

  browseDirectoryButton_->Bind(
      wxEVT_BUTTON, [this](wxCommandEvent&) { onBrowseDirectory(); });
  browseFileButton_->Bind(wxEVT_BUTTON,
                          [this](wxCommandEvent&) { onBrowseFile(); });
  indexButton_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onIndex(); });
  searchButton_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onSearch(); });
  queryCtrl_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { onSearch(); });
  resultsList_->Bind(wxEVT_LIST_ITEM_ACTIVATED,
                     [this](wxListEvent& list_event) {
                       onResultActivated(list_event.GetIndex());
                     });
  Bind(wxEVT_SIZE, [this](wxSizeEvent& size_event) {
    size_event.Skip();
    updateResultColumns();
  });

  loadCurrentIndex();
  updateIndexState();
  if (hasIndex_) {
    showIndexedFiles(SearchLimit);
  } else {
    showNoIndexState();
  }
  updateResultColumns();
}

auto MainFrame::loadCurrentIndex() -> void {
  try {
    const index::IndexRepository::CurrentIndex current_index =
        index::IndexRepository::loadCurrentIndex();
    index::IndexStorage index_storage;
    index_ = index_storage.load(current_index.indexFile);
    rootPath_ = current_index.rootPath;
    indexFile_ = current_index.indexFile;
    hasIndex_ = true;
    pathCtrl_->SetValue(toWxString(rootPath_));
  } catch (const std::exception& exception) {
    hasIndex_ = false;
    rootPath_.clear();
    indexFile_.clear();
    SetStatusText("No current index loaded", 0);
    SetStatusText(toWxString(std::string(exception.what())), 1);
  }
}

auto MainFrame::indexPath() -> void {
  const std::string target_path_text =
      trim(fromWxString(pathCtrl_->GetValue()));
  if (target_path_text.empty()) {
    showError("Index path", "Choose a file or directory to index.");
    return;
  }

  try {
    wxBusyCursor busy_cursor;
    indexButton_->Disable();
    searchButton_->Disable();

    const std::filesystem::path target_path(target_path_text);
    const std::filesystem::path index_file =
        index::IndexRepository::indexFileForPath(target_path);
    index::IndexBuilder index_builder;
    index::IndexBuilder::Result build_result =
        index_builder.build({target_path, index_file, defaultThreadCount()});

    index_ = std::move(build_result.index);
    rootPath_ = std::move(build_result.rootPath);
    indexFile_ = std::move(build_result.indexFile);
    hasIndex_ = true;

    std::ostringstream status_stream;
    status_stream << "Indexed " << rootPath_ << "  Reused "
                  << build_result.reusedTextFiles << "  Parsed "
                  << build_result.parsedTextFiles;
    SetStatusText(toWxString(status_stream.str()), 0);
    updateIndexState();
    runSearch();
  } catch (const std::exception& exception) {
    showError("Index failed", exception.what());
    updateIndexState();
  }
}

auto MainFrame::runSearch() -> void {
  clearResults();
  const std::string query_text = trim(fromWxString(queryCtrl_->GetValue()));
  if (!hasIndex_) {
    showNoIndexState();
    return;
  }

  if (query_text.empty()) {
    showIndexedFiles(SearchLimit);
    return;
  }

  const search::SearchEngine search_engine(index_);
  const int search_mode = searchModeChoice_->GetSelection();
  if (search_mode == 1) {
    const std::vector<search::SearchEngine::GrepLine> grep_lines =
        search_engine.grepLines(query_text, SearchLimit);
    resultEntries_.reserve(grep_lines.size());
    for (const auto& grep_line : grep_lines) {
      const long row_index = resultsList_->InsertItem(
          resultsList_->GetItemCount(), toWxString(grep_line.record.path));
      styleResultRow(resultsList_, row_index);
      resultsList_->SetItem(row_index, 1,
                            toWxString(std::to_string(grep_line.line)));
      resultsList_->SetItem(row_index, 2, toWxString(grep_line.text));
      resultsList_->SetItemData(row_index,
                                static_cast<long>(resultEntries_.size()));
      resultEntries_.push_back({grep_line.record.path, grep_line.line});
    }
  } else {
    const std::vector<index::FileRecord> file_records =
        search_engine.findByName(query_text, SearchLimit);
    resultEntries_.reserve(file_records.size());
    for (const auto& file_record : file_records) {
      const long row_index = resultsList_->InsertItem(
          resultsList_->GetItemCount(), toWxString(file_record.path));
      styleResultRow(resultsList_, row_index);
      resultsList_->SetItem(row_index, 1, wxEmptyString);
      resultsList_->SetItem(row_index, 2,
                            toWxString(sizeText(file_record.size)));
      resultsList_->SetItemData(row_index,
                                static_cast<long>(resultEntries_.size()));
      resultEntries_.push_back({file_record.path, 0});
    }
  }

  SetStatusText(
      toWxString(std::to_string(resultEntries_.size()) + " result(s)"), 0);
}

auto MainFrame::showIndexedFiles(std::size_t limit) -> void {
  clearResults();
  const std::vector<index::FileRecord>& file_records = index_.records();
  const std::size_t visible_count = std::min(limit, file_records.size());
  resultEntries_.reserve(visible_count);

  for (std::size_t record_index = 0; record_index < visible_count;
       ++record_index) {
    const index::FileRecord& file_record = file_records[record_index];
    const long row_index = resultsList_->InsertItem(
        resultsList_->GetItemCount(), toWxString(file_record.path));
    styleResultRow(resultsList_, row_index);
    resultsList_->SetItem(row_index, 1, wxEmptyString);
    resultsList_->SetItem(row_index, 2, toWxString(sizeText(file_record.size)));
    resultsList_->SetItemData(row_index,
                              static_cast<long>(resultEntries_.size()));
    resultEntries_.push_back({file_record.path, 0});
  }

  std::ostringstream status_stream;
  status_stream << "Showing " << visible_count << " of " << file_records.size()
                << " indexed file(s)";
  SetStatusText(toWxString(status_stream.str()), 0);
}

auto MainFrame::showNoIndexState() -> void {
  clearResults();
  const long row_index = resultsList_->InsertItem(resultsList_->GetItemCount(),
                                                  "No current index");
  styleResultRow(resultsList_, row_index);
  resultsList_->SetItem(row_index, 1, wxEmptyString);
  resultsList_->SetItem(row_index, 2, "Index content will appear here");
}

auto MainFrame::updateIndexState() -> void {
  searchButton_->Enable(hasIndex_);
  if (!hasIndex_) {
    SetStatusText("No index loaded", 0);
    SetStatusText("Choose a path and click Index", 1);
    return;
  }

  SetStatusText(toWxString("Root: " + rootPath_), 0);
  SetStatusText(toWxString(indexSummary(index_)), 1);
}

auto MainFrame::updateResultColumns() -> void {
  if (resultsList_ == nullptr) {
    return;
  }

  const int list_width = resultsList_->GetClientSize().GetWidth();
  if (list_width <= 0) {
    return;
  }

  const int available_width = std::max(360, list_width - ListChromeWidth);
  const int line_column_width = std::min(LineColumnWidth, available_width / 5);
  const int flexible_width = available_width - line_column_width;
  const int path_column_width = std::max(220, flexible_width * 55 / 100);
  const int details_column_width =
      std::max(160, flexible_width - path_column_width);

  resultsList_->SetColumnWidth(0, path_column_width);
  resultsList_->SetColumnWidth(1, line_column_width);
  resultsList_->SetColumnWidth(2, details_column_width);
}

auto MainFrame::clearResults() -> void {
  resultEntries_.clear();
  resultsList_->DeleteAllItems();
}

auto MainFrame::showError(const std::string& title,
                          const std::string& message) -> void {
  wxMessageBox(toWxString(message), toWxString(title), wxOK | wxICON_ERROR,
               this);
}

auto MainFrame::onBrowseDirectory() -> void {
  wxDirDialog directory_dialog(this, "Choose directory", pathCtrl_->GetValue(),
                               wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
  if (directory_dialog.ShowModal() == wxID_OK) {
    pathCtrl_->SetValue(directory_dialog.GetPath());
  }
}

auto MainFrame::onBrowseFile() -> void {
  wxFileDialog file_dialog(this, "Choose file", wxEmptyString, wxEmptyString,
                           wxFileSelectorDefaultWildcardStr,
                           wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (file_dialog.ShowModal() == wxID_OK) {
    pathCtrl_->SetValue(file_dialog.GetPath());
  }
}

auto MainFrame::onIndex() -> void { indexPath(); }

auto MainFrame::onSearch() -> void { runSearch(); }

auto MainFrame::onResultActivated(long row_index) -> void {
  if (row_index < 0) {
    return;
  }

  const long result_entry_index = resultsList_->GetItemData(row_index);
  if (result_entry_index < 0 ||
      static_cast<std::size_t>(result_entry_index) >= resultEntries_.size()) {
    return;
  }

  const ResultEntry& result_entry =
      resultEntries_[static_cast<std::size_t>(result_entry_index)];
  if (!wxLaunchDefaultApplication(toWxString(result_entry.path))) {
    showError("Open failed", "Could not open " + result_entry.path.string());
  }
}

}  // namespace minisearch::gui
