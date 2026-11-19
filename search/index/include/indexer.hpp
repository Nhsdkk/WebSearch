#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEXER_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEXER_HPP_

#include <string>
#include <vector>

#include "corpus_reader.hpp"
#include "index.hpp"
#include "tokenizer.hpp"

namespace indexing {
// Builds an index from escaped TSV records: title, URL, text.
class Indexer {
   public:
    Indexer() = default;
    Indexer(const Indexer&) = default;
    Indexer& operator=(const Indexer&) = default;

    // Throws std::runtime_error for invalid records or input failures.
    Index Build(const std::string& corpus_path) const;

   private:
    using SourceDocument = CorpusDocument;

    struct Occurrence {
        std::string term;
        DocId doc = 0;
    };

    struct Corpus {
        std::vector<Document> documents;
        std::vector<Occurrence> occurrences;
        Statistics statistics;
    };

    Corpus ReadCorpus(const std::string& path) const;
    void AddDocument(SourceDocument document, Corpus& corpus) const;
    std::vector<Occurrence> CollectTerms(const SourceDocument& document, DocId id, Statistics& statistics) const;
    std::vector<Term> BuildTerms(std::vector<Occurrence>& occurrences, Statistics& statistics) const;

    static bool Before(const Occurrence& a, const Occurrence& b);
    static void MergeSort(std::vector<Occurrence>& values);

    Tokenizer tokenizer_;
};
}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEXER_HPP_
