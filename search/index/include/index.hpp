#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace indexing {
// In-memory document metadata, sorted terms and posting lists. Tokenization,
// index construction and binary storage are provided by separate classes.
using DocId = std::uint32_t;
using DocList = std::vector<DocId>;

struct Document {
    std::string title;
    std::string url;
};

struct Term {
    std::string text;
    DocList postings;
};

struct IndexingTimings {
    std::uint64_t build_nanoseconds = 0;
    std::uint64_t write_nanoseconds = 0;

    double TotalSeconds() const;
    double MillisecondsPerDocument(std::uint64_t documents) const;
    double MillisecondsPerKib(std::uint64_t text_bytes) const;
    double DocumentsPerSecond(std::uint64_t documents) const;
    double KibPerSecond(std::uint64_t text_bytes) const;
};

struct Statistics {
    std::uint64_t text_bytes = 0;
    std::uint64_t token_count = 0;
    std::uint64_t token_characters = 0;
    std::uint64_t term_characters = 0;
    std::optional<IndexingTimings> indexing;
};

// Owns sorted, unique terms and postings; document IDs index documents().
// Concurrent reads require the index to stay alive and unchanged.
class Index {
   public:
    Index() = default;
    Index(std::vector<Document> documents, std::vector<Term> terms, const Statistics& statistics);

    Index(const Index&) = default;
    Index& operator=(const Index&) = default;
    Index(Index&&) noexcept = default;
    Index& operator=(Index&&) noexcept = default;

    const std::vector<Document>& documents() const { return documents_; }
    const std::vector<Term>& terms() const { return terms_; }
    const Statistics& statistics() const { return statistics_; }

    // Returns postings for a normalized term, or an empty list if it is absent.
    // The reference remains valid until the index is replaced or destroyed.
    const DocList& Lookup(const std::string& term) const;

   private:
    std::vector<Document> documents_;
    std::vector<Term> terms_;
    Statistics statistics_;
    DocList empty_postings_;
};
}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_HPP_
