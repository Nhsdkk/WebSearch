#ifndef WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_STEM_INDEX_HPP_
#define WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_STEM_INDEX_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "index.hpp"
#include "russian_stemmer.hpp"

namespace boolsearch {

// Owns merged postings; no references into a movable index are retained.
class StemIndex {
   public:
    explicit StemIndex(const indexing::Index& index);
    const indexing::DocList& Lookup(const std::string& normalized_token) const;
    std::size_t size() const { return entries_.size(); }

   private:
    struct Source {
        std::string stem;
        std::size_t term = 0;
        static bool Before(const Source& a, const Source& b) {
            return a.stem < b.stem || (a.stem == b.stem && a.term < b.term);
        }
    };
    struct Entry {
        std::string stem;
        indexing::DocList postings;
    };
    static indexing::DocList Merge(const indexing::Index& index, const std::vector<Source>& sources, std::size_t begin,
                                   std::size_t end);
    std::vector<Entry> entries_;
    indexing::DocList empty_;
    morphology::RussianStemmer stemmer_;
};

}  // namespace boolsearch
#endif  // WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_STEM_INDEX_HPP_
