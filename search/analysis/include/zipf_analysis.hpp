#ifndef WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_ZIPF_ANALYSIS_HPP_
#define WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_ZIPF_ANALYSIS_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "index.hpp"

namespace analysis {

class ZipfAnalysis {
   public:
    struct RankedTerm {
        std::size_t term = 0;
        std::uint64_t frequency = 0;

        static bool Before(const RankedTerm& a, const RankedTerm& b) {
            return a.frequency > b.frequency || (a.frequency == b.frequency && a.term < b.term);
        }
    };

    struct Model {
        double c = 0;
        double q = 0;
        double s = 1;
        double log_rmse = 0;

        double Predict(double rank) const;
    };

    struct Report {
        bool available = false;
        std::vector<RankedTerm> terms;
        std::optional<Model> zipf;
        std::optional<Model> mandelbrot;

        std::vector<std::size_t> SampleRanks(std::size_t limit = 2000) const;
    };

    Report Run(const indexing::Index& index) const;

   private:
    static std::vector<RankedTerm> RankTerms(const indexing::Index& index);
    static Model FitZipf(const std::vector<RankedTerm>& terms);
    static Model FitMandelbrot(const std::vector<RankedTerm>& terms);
    static Model Fit(const std::vector<RankedTerm>& terms, double q);
    static double Error(const std::vector<RankedTerm>& terms, const Model& model);
};

}  // namespace analysis
#endif  // WEBSEARCH_SEARCH_ANALYSIS_INCLUDE_ZIPF_ANALYSIS_HPP_
