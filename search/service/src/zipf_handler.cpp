#include <cstddef>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <userver/formats/json.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_status.hpp>
#include <utility>

#include "analysis_response.hpp"
#include "handlers.hpp"
#include "zipf_analysis.hpp"

namespace search_web {
namespace {
using detail::AnalysisResponse;
using userver::formats::json::Type;
using userver::formats::json::ValueBuilder;
using userver::server::http::HttpRequest;
using userver::server::http::HttpStatus;

class ZipfResponse {
   public:
    static ValueBuilder Summary(const IndexSnapshot& snapshot);
    static ValueBuilder Points(const IndexSnapshot& snapshot);
    static std::string Table(const IndexSnapshot& snapshot);

   private:
    static ValueBuilder Model(const std::optional<analysis::ZipfAnalysis::Model>& model);
};
}  // namespace

std::string ZipfHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    const auto snapshot = search_index_.Snapshot();
    auto body = ZipfResponse::Summary(*snapshot);
    if (!snapshot->zipf().available) {
        body["reason"] = "Rebuild the index as BIDX 1.2 to collect term frequencies";
        return AnalysisResponse::Json(request, std::move(body));
    }

    const auto& format = request.GetArg("format");
    if (!format.empty() && format != "tsv") {
        body["error"] = "Format must be tsv or omitted";
        return AnalysisResponse::Json(request, std::move(body), HttpStatus::kBadRequest);
    }
    if (format == "tsv") {
        auto& response = request.GetHttpResponse();
        response.SetContentType(userver::http::ContentType("text/tab-separated-values; charset=utf-8"));
        response.SetHeader(std::string_view("Content-Disposition"), "attachment; filename=zipf.tsv");
        return ZipfResponse::Table(*snapshot);
    }

    body["points"] = ZipfResponse::Points(*snapshot);
    body["fit_ranks"] = snapshot->zipf().terms.size();
    body["frequency_kind"] = "collection_frequency";
    return AnalysisResponse::Json(request, std::move(body));
}

namespace {
ValueBuilder ZipfResponse::Summary(const IndexSnapshot& snapshot) {
    auto body = AnalysisResponse::Metadata(snapshot);
    const auto& report = snapshot.zipf();
    body["available"] = report.available;
    body["terms"] = snapshot.index().terms().size();
    body["tokens"] = snapshot.index().statistics().token_count;
    body["zipf"] = Model(report.zipf);
    body["mandelbrot"] = Model(report.mandelbrot);
    return body;
}

ValueBuilder ZipfResponse::Points(const IndexSnapshot& snapshot) {
    const auto& report = snapshot.zipf();
    ValueBuilder points(Type::kArray);
    for (const auto i : report.SampleRanks()) {
        ValueBuilder point(Type::kObject);
        point["rank"] = i + 1;
        point["term"] = snapshot.index().terms()[report.terms[i].term].text;
        point["frequency"] = report.terms[i].frequency;
        point["zipf"] = report.zipf->Predict(i + 1.0);
        point["mandelbrot"] =
            report.mandelbrot ? ValueBuilder(report.mandelbrot->Predict(i + 1.0)) : ValueBuilder(nullptr);
        points.PushBack(std::move(point));
    }
    return points;
}

std::string ZipfResponse::Table(const IndexSnapshot& snapshot) {
    const auto& report = snapshot.zipf();
    std::ostringstream table;
    table.imbue(std::locale::classic());
    table << "rank\tterm\tfrequency\tzipf\tmandelbrot\n" << std::setprecision(17);
    for (std::size_t i = 0; i < report.terms.size(); ++i) {
        const auto& term = report.terms[i];
        table << i + 1 << '\t' << snapshot.index().terms()[term.term].text << '\t' << term.frequency << '\t';
        if (report.zipf) {
            table << report.zipf->Predict(i + 1.0);
        }
        table << '\t';
        if (report.mandelbrot) {
            table << report.mandelbrot->Predict(i + 1.0);
        }
        table << '\n';
    }
    return table.str();
}

ValueBuilder ZipfResponse::Model(const std::optional<analysis::ZipfAnalysis::Model>& model) {
    if (!model) {
        return ValueBuilder(nullptr);
    }

    ValueBuilder body(Type::kObject);
    body["c"] = model->c;
    body["q"] = model->q;
    body["s"] = model->s;
    body["log10_rmse"] = model->log_rmse;
    return body;
}
}  // namespace
}  // namespace search_web
