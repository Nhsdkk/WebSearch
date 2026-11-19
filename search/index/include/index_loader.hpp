#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_LOADER_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_LOADER_HPP_

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "index.hpp"
#include "index_format.hpp"
#include "tokenizer.hpp"

namespace indexing {
// Loads and validates a BIDX file into an independent in-memory index.
class IndexLoader {
   public:
    IndexLoader() = default;
    IndexLoader(const IndexLoader&) = default;
    IndexLoader& operator=(const IndexLoader&) = default;

    // Throws std::runtime_error for unreadable, corrupt or incompatible files.
    Index Load(const std::string& path) const;

   private:
    using Header = detail::IndexFormat::Header;
    using Section = detail::IndexFormat::Section;

    struct Directory;

    static Directory ReadDirectory(std::istream& input, const Header& header);
    static std::istringstream ReadSection(std::istream& input, const Section& section);

    std::vector<Document> ReadDocuments(std::istream& input, const Section& section) const;
    std::vector<Term> ReadTerms(std::istream& input, const Directory& directory, std::uint64_t& term_characters) const;
    static DocList ReadPostings(std::istream& input, std::uint64_t document_count, std::uint64_t count);
    static Statistics ReadStatistics(std::istream& input, const Directory& directory, std::uint64_t term_characters);

    Tokenizer tokenizer_;
};
}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_LOADER_HPP_
