#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_QUALITY_DATASET_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_QUALITY_DATASET_HPP_

#include <string>

#include "search_quality.hpp"

namespace search_web {
class QualityDatasetReader {
   public:
    analysis::SearchQualityBenchmark::Dataset Load(const std::string& path) const;
};
}  // namespace search_web
#endif  // WEBSEARCH_SEARCH_SERVICE_SRC_QUALITY_DATASET_HPP_
