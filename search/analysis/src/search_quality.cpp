#include "search_quality.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace analysis {

std::vector<SearchQualityBenchmark::Comparison> SearchQualityBenchmark::Run(const indexing::Index& index,
                                                                            const boolsearch::BooleanSearch& search,
                                                                            const Dataset& dataset,
                                                                            const std::string& sha256,
                                                                            std::uint32_t repetitions) const {
    if (repetitions == 0 || repetitions > 100) {
        throw std::invalid_argument("Repetitions must be between 1 and 100");
    }
    dataset.Validate(index, sha256);

    std::vector<Comparison> results;
    for (const auto& sample : dataset.cases) {
        results.push_back(Compare(search, sample, repetitions));
    }
    return results;
}

SearchQualityBenchmark::Comparison SearchQualityBenchmark::Compare(const boolsearch::BooleanSearch& search,
                                                                   const Case& sample, std::uint32_t repetitions) {
    Comparison comparison;
    comparison.id = sample.id;
    comparison.query = sample.query;
    comparison.intent = sample.intent;
    comparison.category = sample.category;

    comparison.exact = Measure(search, sample.query, boolsearch::MatchMode::kExact, repetitions);
    comparison.stem = Measure(search, sample.query, boolsearch::MatchMode::kStem, repetitions);

    comparison.pool_relevant = CountPoolRelevant(sample, comparison);
    Score(comparison.exact, sample, comparison.pool_relevant);
    Score(comparison.stem, sample, comparison.pool_relevant);
    return comparison;
}

SearchQualityBenchmark::Result SearchQualityBenchmark::Measure(const boolsearch::BooleanSearch& search,
                                                               const std::string& query, boolsearch::MatchMode mode,
                                                               std::uint32_t repetitions) {
    const auto warm = search.Search(query, mode);
    Result result;
    result.total = warm.size();
    const auto depth = warm.size() < kDepth ? warm.size() : kDepth;
    result.top.assign(warm.begin(), warm.begin() + depth);

    for (std::uint32_t i = 0; i < repetitions; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const auto matches = search.Search(query, mode);
        result.mean_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

        if (matches != warm) {
            throw std::runtime_error("Search results changed during quality benchmark");
        }
    }
    result.mean_ms /= repetitions;
    return result;
}

std::size_t SearchQualityBenchmark::CountPoolRelevant(const Case& sample, const Comparison& comparison) {
    std::size_t relevant = 0;
    for (const auto& judgment : sample.judgments) {
        bool in_pool = false;
        for (const auto id : comparison.exact.top) {
            if (id == judgment.document) {
                in_pool = true;
            }
        }
        for (const auto id : comparison.stem.top) {
            if (id == judgment.document) {
                in_pool = true;
            }
        }

        if (!in_pool) {
            throw std::invalid_argument("Judgment outside top-20 pool for query " + sample.id);
        }
        if (judgment.relevant) {
            ++relevant;
        }
    }
    return relevant;
}

void SearchQualityBenchmark::Score(Result& result, const Case& sample, std::size_t pool_relevant) {
    for (const auto id : result.top) {
        const Judgment* found = nullptr;
        for (const auto& judgment : sample.judgments) {
            if (judgment.document == id) {
                found = &judgment;
                break;
            }
        }

        if (!found) {
            throw std::invalid_argument("Unjudged top-20 result for query " + sample.id);
        }
        if (found->relevant) {
            ++result.relevant;
        }
    }

    result.precision = static_cast<double>(result.relevant) / kDepth;
    result.pool_recall = pool_relevant == 0 ? 0 : static_cast<double>(result.relevant) / pool_relevant;
}

void SearchQualityBenchmark::Dataset::Validate(const indexing::Index& index, const std::string& sha256) const {
    if (index_sha256 != sha256) {
        throw std::invalid_argument("Quality dataset belongs to a different index");
    }
    if (cases.empty()) {
        throw std::invalid_argument("Quality dataset has no queries");
    }

    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto& sample = cases[i];
        if (sample.id.empty() || sample.query.empty() || sample.intent.empty()) {
            throw std::invalid_argument("Quality queries require ID, query and intent");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (cases[j].id == sample.id) {
                throw std::invalid_argument("Duplicate quality query ID");
            }
        }

        for (std::size_t j = 0; j < sample.judgments.size(); ++j) {
            const auto& judgment = sample.judgments[j];
            if (judgment.document >= index.documents().size() ||
                index.documents()[judgment.document].url != judgment.url || judgment.reason.empty()) {
                throw std::invalid_argument("Invalid quality document identity or missing judgment reason");
            }
            for (std::size_t k = 0; k < j; ++k) {
                if (sample.judgments[k].document == judgment.document) {
                    throw std::invalid_argument("Duplicate document judgment");
                }
            }
        }
    }
}

}  // namespace analysis
