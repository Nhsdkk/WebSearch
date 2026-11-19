#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_HANDLERS_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_HANDLERS_HPP_

#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

#include "search_index.hpp"

namespace search_web {

class IndexHandler : public userver::server::handlers::HttpHandlerBase {
   public:
    IndexHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context);

   protected:
    static userver::formats::json::ValueBuilder Statistics(const IndexSnapshot& snapshot);

    SearchIndex& search_index_;
};

class LoadIndexHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-load-index";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

class SearchHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-search";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

class StatisticsHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-statistics";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

class BenchmarkHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-benchmark";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

class TokenizationBenchmarkHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-tokenization-benchmark";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

class FrontendHandler final : public IndexHandler {
   public:
    static constexpr std::string_view kName = "handler-frontend";
    using IndexHandler::IndexHandler;

    std::string HandleRequest(userver::server::http::HttpRequest& request,
                              userver::server::request::RequestContext&) const override;
};

}  // namespace search_web

#endif  // WEBSEARCH_SEARCH_SERVICE_SRC_HANDLERS_HPP_
