#include "indexer.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "checked_arithmetic.hpp"
#include "index.hpp"
#include "tokenizer.hpp"

namespace indexing {
using detail::CheckedArithmetic;

Index Indexer::Build(const std::string& corpus_path) const {
    auto corpus = ReadCorpus(corpus_path);

    corpus.occurrences.shrink_to_fit();
    MergeSort(corpus.occurrences);
    auto terms = BuildTerms(corpus);

    return Index(std::move(corpus.documents), std::move(terms), corpus.statistics, std::move(corpus.frequencies));
}

Indexer::Corpus Indexer::ReadCorpus(const std::string& path) const {
    CorpusReader reader(path);
    Corpus corpus;
    SourceDocument document;
    while (reader.Next(document)) {
        try {
            AddDocument(std::move(document), corpus);
        } catch (const std::exception& error) {
            throw std::runtime_error("Corpus line " + std::to_string(reader.line_number()) + ": " + error.what());
        }
    }
    return corpus;
}

void Indexer::AddDocument(SourceDocument document, Corpus& corpus) const {
    if (corpus.documents.size() >= CheckedArithmetic::kMaxUint32) {
        throw std::runtime_error("Too many documents for 32-bit IDs");
    }

    const auto id = static_cast<DocId>(corpus.documents.size());
    auto terms = CollectTerms(document, id, corpus.statistics);
    MergeSort(terms);

    // Keep one occurrence per document; statistics count all original tokens.
    for (auto& term : terms) {
        if (corpus.occurrences.empty() || corpus.occurrences.back().doc != id ||
            corpus.occurrences.back().term != term.term) {
            corpus.occurrences.push_back(std::move(term));
        } else {
            corpus.occurrences.back().count = CheckedArithmetic::Add(corpus.occurrences.back().count, term.count);
        }
    }
    corpus.documents.push_back({std::move(document.title), std::move(document.url)});
}

std::vector<Indexer::Occurrence> Indexer::CollectTerms(const SourceDocument& document, DocId id,
                                                       Statistics& statistics) const {
    const auto text_bytes = CheckedArithmetic::Add(document.title.size(), document.text.size());
    statistics.text_bytes = CheckedArithmetic::Add(statistics.text_bytes, text_bytes);

    std::vector<Occurrence> terms;
    for (const auto* field : {&document.title, &document.text}) {
        auto tokens = tokenizer_.Tokenize(*field);
        statistics.token_count = CheckedArithmetic::Add(statistics.token_count, tokens.size());

        for (auto& token : tokens) {
            statistics.token_characters =
                CheckedArithmetic::Add(statistics.token_characters, tokenizer_.Utf8Length(token));
            terms.push_back({std::move(token), id});
        }
    }
    return terms;
}

std::vector<Term> Indexer::BuildTerms(Corpus& corpus) const {
    std::vector<Term> terms;
    for (auto& occurrence : corpus.occurrences) {
        if (terms.empty() || terms.back().text != occurrence.term) {
            corpus.statistics.term_characters =
                CheckedArithmetic::Add(corpus.statistics.term_characters, tokenizer_.Utf8Length(occurrence.term));
            terms.push_back({std::move(occurrence.term), {occurrence.doc}});
            corpus.frequencies.push_back(occurrence.count);
        } else {
            if (terms.back().postings.back() != occurrence.doc) {
                terms.back().postings.push_back(occurrence.doc);
            }
            corpus.frequencies.back() = CheckedArithmetic::Add(corpus.frequencies.back(), occurrence.count);
        }
    }
    return terms;
}

bool Indexer::Before(const Occurrence& a, const Occurrence& b) {
    return a.term < b.term || (a.term == b.term && a.doc < b.doc);
}

// Bottom-up stable merge sort; strings are moved between two flat arrays.
void Indexer::MergeSort(std::vector<Occurrence>& values) {
    std::vector<Occurrence> buffer(values.size());
    for (std::size_t width = 1; width < values.size();) {
        for (std::size_t start = 0; start < values.size();) {
            const std::size_t middle = start + std::min(width, values.size() - start);
            const std::size_t end = middle + std::min(width, values.size() - middle);
            std::size_t left = start;
            std::size_t right = middle;
            std::size_t destination = start;

            while (left < middle && right < end) {
                if (Before(values[right], values[left])) {
                    buffer[destination++] = std::move(values[right++]);
                } else {
                    buffer[destination++] = std::move(values[left++]);
                }
            }

            while (left < middle) {
                buffer[destination++] = std::move(values[left++]);
            }
            while (right < end) {
                buffer[destination++] = std::move(values[right++]);
            }
            start = end;
        }

        values.swap(buffer);
        if (width > values.size() / 2) {
            break;
        }
        width *= 2;
    }
}

}  // namespace indexing
