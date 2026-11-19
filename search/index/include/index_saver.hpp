#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_SAVER_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_SAVER_HPP_

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "index.hpp"

namespace indexing {
// Saves an index in the BIDX binary format.
class IndexSaver {
   public:
    IndexSaver() = default;
    IndexSaver(const IndexSaver&) = default;
    IndexSaver& operator=(const IndexSaver&) = default;

    // Replaces the file. Throws on overflow or I/O failure; failed writes may
    // leave a partial file.
    // Providing the build duration also records the duration of writing and
    // flushing the file, before updating the timing record.
    std::optional<IndexingTimings> Save(const Index& index, const std::string& path,
                                        std::optional<std::uint64_t> build_nanoseconds = std::nullopt) const;

   private:
    struct Layout;

    static Layout CalculateLayout(const Index& index, bool include_indexing);
    static void WriteDirectory(std::ostream& out, const Layout& layout);
    static void WriteDocuments(std::ostream& out, const std::vector<Document>& documents);
    static void WriteTerms(std::ostream& out, const std::vector<Term>& terms);
    static void WritePostings(std::ostream& out, const std::vector<Term>& terms);
    static void WriteStatistics(std::ostream& out, const Statistics& statistics);
};
}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_SAVER_HPP_
