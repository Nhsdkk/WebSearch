#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/utils/daemon_run.hpp>

#include "handlers.hpp"
#include "search_index.hpp"

int main(int argc, char* argv[]) {
    const auto components = userver::components::MinimalServerComponentList()
                                .Append<search_web::SearchIndex>()
                                .Append<search_web::LoadIndexHandler>()
                                .Append<search_web::SearchHandler>()
                                .Append<search_web::StatisticsHandler>()
                                .Append<search_web::BenchmarkHandler>()
                                .Append<search_web::TokenizationBenchmarkHandler>()
                                .Append<search_web::FrontendHandler>();
    return userver::utils::DaemonMain(argc, argv, components);
}
