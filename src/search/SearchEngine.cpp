#include "minisearch/search/SearchEngine.hpp"

#include "minisearch/index/TextParser.hpp"

#include <sstream>

namespace minisearch::search {

SearchEngine::SearchEngine(const index::InvertedIndex& index)
    : index_(index)
{
}

std::vector<index::FileRecord> SearchEngine::findByName(const std::string& query,
                                                        std::size_t limit) const
{
    return index_.findByName(query, limit);
}

std::vector<index::FileRecord> SearchEngine::grep(const std::string& query,
                                                  std::size_t limit) const
{
    return index_.findByTerms(index::TextParser::tokenize(query), limit);
}

std::string SearchEngine::formatRecord(const index::FileRecord& record)
{
    std::ostringstream stream;
    stream << record.path.string() << " (" << record.size << " bytes)";
    return stream.str();
}

} // namespace minisearch::search

