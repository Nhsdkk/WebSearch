#include "zipf_analysis.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include "manual_merge_sort.hpp"

namespace analysis {

ZipfAnalysis::Report ZipfAnalysis::Run(const indexing::Index& index) const {
    Report report;
    if (!index.frequencies()) {
        return report;
    }

    report.terms = RankTerms(index);
    report.available = true;
    if (report.terms.empty()) {
        return report;
    }

    report.zipf = FitZipf(report.terms);
    if (report.terms.size() >= 3) {
        const auto candidate = FitMandelbrot(report.terms);
        if (std::isfinite(candidate.log_rmse)) {
            report.mandelbrot = candidate;
        }
    }
    return report;
}

std::vector<std::size_t> ZipfAnalysis::Report::SampleRanks(std::size_t limit) const {
    std::vector<std::size_t> ranks;
    if (terms.empty() || limit == 0) {
        return ranks;
    }

    if (terms.size() <= limit) {
        for (std::size_t i = 0; i < terms.size(); ++i) {
            ranks.push_back(i);
        }
        return ranks;
    }
    if (limit == 1) {
        return {0};
    }

    for (std::size_t i = 0; i < limit; ++i) {
        auto rank = terms.size() - 1;
        if (i + 1 < limit) {
            const auto log_position = std::log(static_cast<double>(terms.size())) * static_cast<double>(i) / (limit - 1);
            rank = static_cast<std::size_t>(std::exp(log_position)) - 1;
        }
        if (ranks.empty() || rank != ranks.back()) {
            ranks.push_back(rank);
        }
    }
    return ranks;
}

double ZipfAnalysis::Model::Predict(double rank) const { return c / std::pow(rank + q, s); }

std::vector<ZipfAnalysis::RankedTerm> ZipfAnalysis::RankTerms(const indexing::Index& index) {
    if (index.frequencies()->size() != index.terms().size()) {
        throw std::runtime_error("Frequency count mismatch");
    }

    std::vector<RankedTerm> terms;
    for (std::size_t i = 0; i < index.terms().size(); ++i) {
        const auto count = index.frequencies()->at(i);
        if (count == 0) {
            throw std::runtime_error("Zero collection frequency");
        }
        terms.push_back({i, count});
    }
    search_common::ManualMergeSort::Sort(terms);
    return terms;
}

ZipfAnalysis::Model ZipfAnalysis::FitZipf(const std::vector<RankedTerm>& terms) {
    long double log_c = 0;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        log_c += std::log(static_cast<double>(terms[i].frequency)) + std::log(i + 1.0);
    }

    Model model{std::exp(static_cast<double>(log_c / terms.size())), 0, 1, 0};
    model.log_rmse = Error(terms, model);
    return model;
}

ZipfAnalysis::Model ZipfAnalysis::FitMandelbrot(const std::vector<RankedTerm>& terms) {
    constexpr std::size_t kGrid = 64;
    constexpr int kRefinements = 32;
    constexpr double kGolden = 0.6180339887498949;
    const auto maximum = static_cast<double>(terms.size());

    // Find a promising interval on a logarithmic grid over q.
    auto best = Fit(terms, 0);
    std::size_t best_grid = 0;
    for (std::size_t i = 1; i <= kGrid; ++i) {
        const auto q = std::expm1(std::log1p(maximum) * i / kGrid);
        const auto candidate = Fit(terms, q);
        if (candidate.log_rmse < best.log_rmse) {
            best = candidate;
            best_grid = i;
        }
    }

    // Refine that interval with golden-section search.
    double left = best_grid == 0 ? 0 : std::expm1(std::log1p(maximum) * (best_grid - 1) / kGrid);
    double right = best_grid == kGrid ? maximum : std::expm1(std::log1p(maximum) * (best_grid + 1) / kGrid);
    auto left_model = Fit(terms, right - kGolden * (right - left));
    auto right_model = Fit(terms, left + kGolden * (right - left));
    for (int i = 0; i < kRefinements; ++i) {
        if (left_model.log_rmse < best.log_rmse) {
            best = left_model;
        }
        if (right_model.log_rmse < best.log_rmse) {
            best = right_model;
        }

        if (left_model.log_rmse < right_model.log_rmse) {
            right = right_model.q;
            right_model = left_model;
            left_model = Fit(terms, right - kGolden * (right - left));
        } else {
            left = left_model.q;
            left_model = right_model;
            right_model = Fit(terms, left + kGolden * (right - left));
        }
    }
    return best;
}

ZipfAnalysis::Model ZipfAnalysis::Fit(const std::vector<RankedTerm>& terms, double q) {
    long double sum_x = 0;
    long double sum_y = 0;
    long double sum_xx = 0;
    long double sum_xy = 0;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const long double log_rank = std::log(i + 1.0 + q);
        const long double log_frequency = std::log(static_cast<double>(terms[i].frequency));
        sum_x += log_rank;
        sum_y += log_frequency;
        sum_xx += log_rank * log_rank;
        sum_xy += log_rank * log_frequency;
    }

    const long double count = terms.size();
    const auto variance = sum_xx - sum_x * sum_x / count;
    if (variance <= 0) {
        return {0, q, 1, std::numeric_limits<double>::infinity()};
    }
    const auto s = static_cast<double>(-(sum_xy - sum_x * sum_y / count) / variance);
    const auto c = std::exp(static_cast<double>((sum_y + s * sum_x) / count));

    Model model{c, q, s, std::numeric_limits<double>::infinity()};
    if (s > 0 && std::isfinite(c) && c > 0) {
        model.log_rmse = Error(terms, model);
    }
    return model;
}

double ZipfAnalysis::Error(const std::vector<RankedTerm>& terms, const Model& model) {
    long double sum = 0;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const auto difference = (std::log(model.c) - model.s * std::log(i + 1.0 + model.q) -
                                 std::log(static_cast<double>(terms[i].frequency))) /
                                std::log(10.0);
        sum += difference * difference;
    }
    return std::sqrt(static_cast<double>(sum / terms.size()));
}

}  // namespace analysis
