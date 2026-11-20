#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <userver/formats/json.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_status.hpp>
#include <utility>
#include <vector>

#include "analysis_response.hpp"
#include "handlers.hpp"
#include "quality_dataset.hpp"
#include "search_quality.hpp"

namespace search_web {
namespace {
using detail::AnalysisResponse;
using userver::formats::json::Type;
using userver::formats::json::ValueBuilder;
using userver::server::http::HttpRequest;
using userver::server::http::HttpStatus;

class QualityResponse {
   public:
    static void AddResults(ValueBuilder& body, const std::vector<analysis::SearchQualityBenchmark::Comparison>& results);

   private:
    static ValueBuilder Result(const analysis::SearchQualityBenchmark::Result& result);
};
}  // namespace

std::string QualityHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    const auto snapshot = search_index_.Snapshot();
    const bool configuration = request.GetMethod() == userver::server::http::HttpMethod::kGet ||
                               request.GetMethod() == userver::server::http::HttpMethod::kHead;
    auto body = AnalysisResponse::Metadata(*snapshot);
    try {
        const auto dataset = QualityDatasetReader{}.Load(search_index_.quality_dataset_path());
        dataset.Validate(snapshot->index(), snapshot->sha256());

        body["available"] = true;
        body["depth"] = analysis::SearchQualityBenchmark::kDepth;
        body["query_count"] = dataset.cases.size();
        body["repetitions"] = 3;
        body["max_repetitions"] = 100;
        if (configuration) {
            return AnalysisResponse::Json(request, std::move(body));
        }

        std::uint32_t repetitions = 3;
        if (!request.RequestBody().empty()) {
            const auto options = userver::formats::json::FromString(request.RequestBody());
            if (!options.IsObject()) {
                throw std::invalid_argument("Benchmark body must be an object");
            }
            if (options.HasMember("repetitions")) {
                repetitions = options["repetitions"].As<std::uint32_t>();
            }
        }

        const auto results = analysis::SearchQualityBenchmark{}.Run(snapshot->index(), snapshot->engine(), dataset,
                                                                    snapshot->sha256(), repetitions);
        QualityResponse::AddResults(body, results);
        body["repetitions"] = repetitions;
        body["warmup_per_query_mode"] = 1;
        return AnalysisResponse::Json(request, std::move(body));
    } catch (const std::exception& error) {
        body["available"] = false;
        body[configuration ? "reason" : "error"] = error.what();
        return AnalysisResponse::Json(request, std::move(body),
                                      configuration ? HttpStatus::kOk : HttpStatus::kBadRequest);
    }
}

namespace {
void QualityResponse::AddResults(ValueBuilder& body,
                                 const std::vector<analysis::SearchQualityBenchmark::Comparison>& results) {
    ValueBuilder rows(Type::kArray);
    double exact_precision = 0;
    double stem_precision = 0;
    double exact_recall = 0;
    double stem_recall = 0;
    for (const auto& result : results) {
        ValueBuilder row(Type::kObject);
        row["id"] = result.id;
        row["query"] = result.query;
        row["intent"] = result.intent;
        row["category"] = result.category;
        row["pool_relevant"] = result.pool_relevant;
        row["exact"] = Result(result.exact);
        row["stem"] = Result(result.stem);
        rows.PushBack(std::move(row));

        exact_precision += result.exact.precision;
        stem_precision += result.stem.precision;
        exact_recall += result.exact.pool_recall;
        stem_recall += result.stem.pool_recall;
    }

    ValueBuilder macro(Type::kObject);
    macro["exact_p_at20"] = exact_precision / results.size();
    macro["stem_p_at20"] = stem_precision / results.size();
    macro["exact_pool_recall_at20"] = exact_recall / results.size();
    macro["stem_pool_recall_at20"] = stem_recall / results.size();
    body["macro"] = std::move(macro);
    body["queries"] = std::move(rows);
}

ValueBuilder QualityResponse::Result(const analysis::SearchQualityBenchmark::Result& result) {
    ValueBuilder body(Type::kObject);
    body["total"] = result.total;
    body["relevant_at20"] = result.relevant;
    body["p_at20"] = result.precision;
    body["pool_recall_at20"] = result.pool_recall;
    body["mean_ms"] = result.mean_ms;

    ValueBuilder top(Type::kArray);
    for (const auto id : result.top) {
        top.PushBack(ValueBuilder(id));
    }
    body["top_ids"] = std::move(top);
    return body;
}
}  // namespace
}  // namespace search_web
