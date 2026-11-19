#include "index.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace indexing {
double IndexingTimings::TotalSeconds() const {
    return (static_cast<double>(build_nanoseconds) + static_cast<double>(write_nanoseconds)) / 1e9;
}

double IndexingTimings::MillisecondsPerDocument(std::uint64_t documents) const {
    return documents == 0 ? 0.0 : TotalSeconds() * 1000.0 / static_cast<double>(documents);
}

double IndexingTimings::MillisecondsPerKib(std::uint64_t text_bytes) const {
    return text_bytes == 0 ? 0.0 : TotalSeconds() * 1000.0 / (static_cast<double>(text_bytes) / 1024.0);
}

double IndexingTimings::DocumentsPerSecond(std::uint64_t documents) const {
    const auto seconds = TotalSeconds();
    return seconds == 0 ? 0.0 : static_cast<double>(documents) / seconds;
}

double IndexingTimings::KibPerSecond(std::uint64_t text_bytes) const {
    const auto seconds = TotalSeconds();
    return seconds == 0 ? 0.0 : static_cast<double>(text_bytes) / 1024.0 / seconds;
}

Index::Index(std::vector<Document> documents, std::vector<Term> terms, const Statistics& statistics)
    : documents_(std::move(documents)), terms_(std::move(terms)), statistics_(statistics) {}

const DocList& Index::Lookup(const std::string& term) const {
    std::size_t left = 0;
    std::size_t right = terms_.size();

    while (left < right) {
        const std::size_t middle = left + (right - left) / 2;
        if (terms_[middle].text < term) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left < terms_.size() && terms_[left].text == term ? terms_[left].postings : empty_postings_;
}
}  // namespace indexing
