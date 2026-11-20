#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_ANALYSIS_RESPONSE_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_ANALYSIS_RESPONSE_HPP_

#include <string>
#include <userver/formats/json.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_status.hpp>

#include "index_store.hpp"

namespace search_web::detail {

class AnalysisResponse {
   public:
    static userver::formats::json::ValueBuilder Metadata(const IndexSnapshot& snapshot) {
        userver::formats::json::ValueBuilder body(userver::formats::json::Type::kObject);
        body["index_name"] = snapshot.name();
        body["index_sha256"] = snapshot.sha256();
        return body;
    }

    static std::string Json(userver::server::http::HttpRequest& request, userver::formats::json::ValueBuilder body,
                            userver::server::http::HttpStatus status = userver::server::http::HttpStatus::kOk) {
        auto& response = request.GetHttpResponse();
        response.SetContentType(userver::http::content_type::kApplicationJson);
        response.SetStatus(status);
        return userver::formats::json::ToString(body.ExtractValue());
    }
};

}  // namespace search_web::detail
#endif  // WEBSEARCH_SEARCH_SERVICE_SRC_ANALYSIS_RESPONSE_HPP_
