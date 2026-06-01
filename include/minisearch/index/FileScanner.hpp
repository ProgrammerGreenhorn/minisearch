#pragma once

#include "minisearch/index/FileRecord.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace minisearch::index {

class FileScanner {
public:
    struct Options {
        std::vector<std::string> textExtensions{
            ".txt", ".md", ".markdown", ".cpp", ".cc", ".cxx", ".c",
            ".hpp", ".hh", ".hxx", ".h", ".json", ".yaml", ".yml",
            ".toml", ".ini", ".cmake"
        };
        std::vector<std::string> excludedNames{
            ".git", "build", "cmake-build-debug", "cmake-build-release",
            "node_modules", ".idea", ".vscode"
        };
        std::uintmax_t maxTextFileBytes = 1024 * 1024;
    };

    FileScanner();
    
    explicit FileScanner(Options options);

    std::vector<FileRecord> scan(const std::filesystem::path& root) const;

private:
    bool shouldSkip(const std::filesystem::directory_entry& entry) const;
    bool shouldIndexText(const std::filesystem::path& path, std::uintmax_t size) const;
    static std::time_t toTimeT(std::filesystem::file_time_type value);
    static std::string lower(std::string value);

    Options options_;
};

} // namespace minisearch::index
