#include "boolean_search/query.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include "index.hpp"
#include "query_parser.hpp"

namespace boolsearch {
using indexing::DocId;
using indexing::DocList;
using indexing::Index;

BooleanSearch::BooleanSearch(const Index& index) : index_(index) {}

DocList BooleanSearch::Search(const std::string& query) const { return Parser(index_, query).Run(); }

DocList BooleanSearch::Intersect(const DocList& a, const DocList& b) {
    DocList result;
    result.reserve(std::min(a.size(), b.size()));
    std::size_t i = 0;
    std::size_t j = 0;

    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) {
            ++i;
        } else if (b[j] < a[i]) {
            ++j;
        } else {
            result.push_back(a[i]);
            ++i;
            ++j;
        }
    }

    return result;
}

DocList BooleanSearch::Unite(const DocList& a, const DocList& b) {
    DocList result;
    result.reserve(a.size() + b.size());
    std::size_t i = 0;
    std::size_t j = 0;

    while (i < a.size() || j < b.size()) {
        if (j == b.size() || (i < a.size() && a[i] < b[j])) {
            result.push_back(a[i++]);
        } else if (i == a.size() || b[j] < a[i]) {
            result.push_back(b[j++]);
        } else {
            result.push_back(a[i]);
            ++i;
            ++j;
        }
    }

    return result;
}

DocList BooleanSearch::Complement(const DocList& values, std::size_t document_count) {
    DocList result;
    result.reserve(document_count - values.size());
    std::size_t j = 0;

    for (std::size_t i = 0; i < document_count; ++i) {
        if (j < values.size() && values[j] == i) {
            ++j;
        } else {
            result.push_back(static_cast<DocId>(i));
        }
    }

    return result;
}
}  // namespace boolsearch
