#include "minisearch/index/FileScanner.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace minisearch::index {

FileScanner::FileScanner()
    : FileScanner(Options{})
{
}

FileScanner::FileScanner(Options options)
    : options_(std::move(options))
{
}

std::vector<FileRecord> FileScanner::scan(const std::filesystem::path& root) const
{
    namespace fs = std::filesystem;

    if (!fs::exists(root)) {
        throw std::runtime_error("path does not exist: " + root.string());
    }

    std::vector<FileRecord> records;
    const auto base = fs::absolute(root);

    if (fs::is_regular_file(base)) {
        const auto size = fs::file_size(base);
        records.emplace_back(
            base,
            size,
            toTimeT(fs::last_write_time(base)),
            shouldIndexText(base, size)
        );
        return records;
    }

    const auto iteratorOptions = fs::directory_options::skip_permission_denied;
    auto it = fs::recursive_directory_iterator(base, iteratorOptions);
    auto end = fs::recursive_directory_iterator{};
    for (; it != end; ++it) {
        const auto& entry = *it;
        if (shouldSkip(entry)) {
            if (entry.is_directory()) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        std::error_code error;
        const auto size = entry.file_size(error);
        if (error) {
            continue;
        }

        records.emplace_back(
            entry.path(),
            size,
            toTimeT(entry.last_write_time()),
            shouldIndexText(entry.path(), size)
        );
    }

    return records;
}

bool FileScanner::shouldSkip(const std::filesystem::directory_entry& entry) const
{
    const auto name = entry.path().filename().string();
    return std::find(options_.excludedNames.begin(), options_.excludedNames.end(), name)
        != options_.excludedNames.end();
}

bool FileScanner::shouldIndexText(const std::filesystem::path& path, std::uintmax_t size) const
{
    if (size > options_.maxTextFileBytes) {
        return false;
    }

    const auto extension = lower(path.extension().string());
    return std::find(options_.textExtensions.begin(), options_.textExtensions.end(), extension)
        != options_.textExtensions.end();
}

std::time_t FileScanner::toTimeT(std::filesystem::file_time_type value)
{
    using namespace std::chrono;
    const auto systemTime = time_point_cast<system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(systemTime);
}

std::string FileScanner::lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace minisearch::index
