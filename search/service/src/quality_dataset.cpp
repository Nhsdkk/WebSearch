#include "quality_dataset.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <userver/formats/json.hpp>
#include <utility>

namespace search_web {

analysis::SearchQualityBenchmark::Dataset QualityDatasetReader::Load(const std::string& path) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Quality judgments are not configured for this corpus");
    }
    const std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) {
        throw std::runtime_error("Cannot read quality dataset");
    }

    const auto root = userver::formats::json::FromString(bytes);
    analysis::SearchQualityBenchmark::Dataset dataset;
    dataset.index_sha256 = root["index_sha256"].As<std::string>();
    if (!root["queries"].IsArray()) {
        throw std::invalid_argument("Quality queries must be an array");
    }

    for (const auto& item : root["queries"]) {
        analysis::SearchQualityBenchmark::Case sample;
        sample.id = item["id"].As<std::string>();
        sample.query = item["query"].As<std::string>();
        sample.intent = item["intent"].As<std::string>();
        sample.category = item["category"].As<std::string>();

        if (!item["judgments"].IsArray()) {
            throw std::invalid_argument("Judgments must be an array");
        }
        for (const auto& judgment : item["judgments"]) {
            sample.judgments.push_back({judgment["document_id"].As<std::uint32_t>(), judgment["url"].As<std::string>(),
                                        judgment["relevant"].As<bool>(), judgment["reason"].As<std::string>()});
        }

        dataset.cases.push_back(std::move(sample));
    }
    return dataset;
}

}  // namespace search_web
