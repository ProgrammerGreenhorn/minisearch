#include "minisearch/index/IndexStorage.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace minisearch::index {

namespace {

constexpr const char* Header = "MINISEARCH_INDEX_V1";

} // namespace

void IndexStorage::save(const std::filesystem::path& path, const InvertedIndex& index) const
{
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open index file for writing: " + path.string());
    }

    output << Header << '\n';
    output << "records " << index.records().size() << '\n';
    for (const auto& record : index.records()) {
        output << record.id << ' '
               << record.size << ' '
               << record.modifiedTime << ' '
               << (record.textIndexed ? 1 : 0) << ' '
               << std::quoted(record.path.string()) << '\n';
    }

    output << "terms " << index.postings().size() << '\n';
    for (const auto& [term, postings] : index.postings()) {
        output << std::quoted(term) << ' ' << postings.size();
        for (const auto id : postings) {
            output << ' ' << id;
        }
        output << '\n';
    }
}

InvertedIndex IndexStorage::load(const std::filesystem::path& path) const
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open index file: " + path.string());
    }

    std::string header;
    std::getline(input, header);
    if (header != Header) {
        throw std::runtime_error("unsupported index format: " + path.string());
    }

    std::string section;
    std::size_t recordCount = 0;
    input >> section >> recordCount;
    if (section != "records") {
        throw std::runtime_error("corrupt index: records section is missing");
    }

    std::vector<FileRecord> records;
    records.reserve(recordCount);
    for (std::size_t i = 0; i < recordCount; ++i) {
        FileRecord record;
        int textIndexed = 0;
        std::string pathString;
        input >> record.id
              >> record.size
              >> record.modifiedTime
              >> textIndexed
              >> std::quoted(pathString);
        record.textIndexed = textIndexed != 0;
        record.path = pathString;
        records.push_back(std::move(record));
    }

    std::size_t termCount = 0;
    input >> section >> termCount;
    if (section != "terms") {
        throw std::runtime_error("corrupt index: terms section is missing");
    }

    std::unordered_map<std::string, InvertedIndex::PostingList> postings;
    postings.reserve(termCount);
    for (std::size_t i = 0; i < termCount; ++i) {
        std::string term;
        std::size_t postingCount = 0;
        input >> std::quoted(term) >> postingCount;

        InvertedIndex::PostingList ids;
        ids.reserve(postingCount);
        for (std::size_t j = 0; j < postingCount; ++j) {
            InvertedIndex::DocumentId id = 0;
            input >> id;
            ids.push_back(id);
        }
        postings.emplace(std::move(term), std::move(ids));
    }

    InvertedIndex index;
    index.replace(std::move(records), std::move(postings));
    return index;
}

} // namespace minisearch::index

