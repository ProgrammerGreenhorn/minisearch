#include "minisearch/search/SearchEngine.hpp"

#include "minisearch/index/TextParser.hpp"

#include <sstream>

namespace minisearch::search {

SearchEngine::SearchEngine(const index::InvertedIndex& index)
    : index_(index)
{
}

auto SearchEngine::findByName(const std::string& query,
                              std::size_t limit) const -> std::vector<index::FileRecord>
{
    return index_.findByName(query, limit);
}

auto SearchEngine::grep(const std::string& query,
                        std::size_t limit) const -> std::vector<index::FileRecord>
{
    return index_.findByTerms(index::TextParser::tokenize(query), limit);
}

auto SearchEngine::formatRecord(const index::FileRecord& record) -> std::string
{
    std::ostringstream stream;
    stream << record.path.string() << " (" << record.size << " bytes)";
    return stream.str();
}

} // namespace minisearch::search
