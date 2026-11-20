#ifndef WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_SEARCH_QUALITY_HPP_
#define WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_SEARCH_QUALITY_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "boolean_search/query.hpp"
#include "index.hpp"

namespace analysis {

class SearchQualityBenchmark {
   public:
    static constexpr std::size_t kDepth = 20;

    struct Judgment {
        indexing::DocId document = 0;
        std::string url;
        bool relevant = false;
        std::string reason;
    };

    struct Case {
        std::string id;
        std::string query;
        std::string intent;
        std::string category;
        std::vector<Judgment> judgments;
    };

    struct Dataset {
        std::string index_sha256;
        std::vector<Case> cases;

        void Validate(const indexing::Index& index, const std::string& sha256) const;
    };

    struct Result {
        std::size_t total = 0;
        std::size_t relevant = 0;
        double precision = 0;
        double pool_recall = 0;
        double mean_ms = 0;
        indexing::DocList top;
    };

    struct Comparison {
        std::string id;
        std::string query;
        std::string intent;
        std::string category;
        std::size_t pool_relevant = 0;
        Result exact;
        Result stem;
    };

    std::vector<Comparison> Run(const indexing::Index& index, const boolsearch::BooleanSearch& search,
                                const Dataset& dataset, const std::string& sha256, std::uint32_t repetitions = 3) const;

   private:
    static Comparison Compare(const boolsearch::BooleanSearch& search, const Case& sample, std::uint32_t repetitions);
    static Result Measure(const boolsearch::BooleanSearch& search, const std::string& query, boolsearch::MatchMode mode,
                          std::uint32_t repetitions);
    static std::size_t CountPoolRelevant(const Case& sample, const Comparison& comparison);
    static void Score(Result& result, const Case& sample, std::size_t pool_relevant);
};

}  // namespace analysis
#endif  // WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_SEARCH_QUALITY_HPP_
