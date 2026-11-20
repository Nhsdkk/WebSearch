#ifndef WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_QUERY_HPP_
#define WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_QUERY_HPP_

#include <cstddef>
#include <string>

#include "boolean_search/match_mode.hpp"
#include "boolean_search/stem_index.hpp"
#include "index.hpp"

namespace boolsearch {
// Searches a borrowed index, which must stay alive and unchanged.
// Concurrent calls use separate parsers and result lists.
class BooleanSearch {
   public:
    explicit BooleanSearch(const indexing::Index& index);
    BooleanSearch(indexing::Index&&) = delete;
    BooleanSearch(const indexing::Index&&) = delete;

    // Copies and moves preserve the index reference.
    BooleanSearch(const BooleanSearch&) = default;
    BooleanSearch& operator=(const BooleanSearch&) = delete;
    BooleanSearch(BooleanSearch&&) noexcept = default;
    BooleanSearch& operator=(BooleanSearch&&) = delete;

    // Returns matching IDs in ascending order. An empty query has no matches.
    // Throws std::runtime_error for invalid syntax, UTF-8, or query limits.
    indexing::DocList Search(const std::string& query, MatchMode mode = MatchMode::kStem) const;
    std::size_t stem_count() const { return stems_.size(); }

   private:
    class Parser;

    static indexing::DocList Intersect(const indexing::DocList& a, const indexing::DocList& b);
    static indexing::DocList Unite(const indexing::DocList& a, const indexing::DocList& b);
    static indexing::DocList Complement(const indexing::DocList& values, std::size_t document_count);

    const indexing::Index& index_;
    StemIndex stems_;
};
}  // namespace boolsearch

#endif  // WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_QUERY_HPP_
