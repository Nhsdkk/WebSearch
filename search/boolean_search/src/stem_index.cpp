#include "boolean_search/stem_index.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "index.hpp"
#include "manual_merge_sort.hpp"

namespace boolsearch {

StemIndex::StemIndex(const indexing::Index& index) {
    std::vector<Source> sources;
    sources.reserve(index.terms().size());
    for (std::size_t i = 0; i < index.terms().size(); ++i) {
        sources.push_back({stemmer_.Stem(index.terms()[i].text), i});
    }

    search_common::ManualMergeSort::Sort(sources);
    for (std::size_t begin = 0; begin < sources.size();) {
        auto end = begin + 1;
        while (end < sources.size() && sources[end].stem == sources[begin].stem) {
            ++end;
        }

        auto postings = Merge(index, sources, begin, end);
        entries_.push_back({std::move(sources[begin].stem), std::move(postings)});
        begin = end;
    }
}

const indexing::DocList& StemIndex::Lookup(const std::string& normalized_token) const {
    const auto stem = stemmer_.Stem(normalized_token);

    std::size_t left = 0;
    auto right = entries_.size();
    while (left < right) {
        const auto middle = left + (right - left) / 2;
        if (entries_[middle].stem < stem) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    return left < entries_.size() && entries_[left].stem == stem ? entries_[left].postings : empty_;
}

indexing::DocList StemIndex::Merge(const indexing::Index& index, const std::vector<Source>& sources, std::size_t begin,
                                   std::size_t end) {
    if (end - begin == 1) {
        return index.terms()[sources[begin].term].postings;
    }

    const auto middle = begin + (end - begin) / 2;
    auto left = Merge(index, sources, begin, middle);
    auto right = Merge(index, sources, middle, end);

    indexing::DocList result;
    result.reserve(left.size() + right.size());
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < left.size() || j < right.size()) {
        if (j == right.size() || (i < left.size() && left[i] < right[j])) {
            result.push_back(left[i++]);
        } else if (i == left.size() || right[j] < left[i]) {
            result.push_back(right[j++]);
        } else {
            result.push_back(left[i++]);
            ++j;
        }
    }
    return result;
}

}  // namespace boolsearch
