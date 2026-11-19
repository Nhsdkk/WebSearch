#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_SEARCH_INDEX_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_SEARCH_INDEX_HPP_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/schema.hpp>
#include <vector>

#include "index_store.hpp"

namespace search_web {

class SearchIndex final : public userver::components::ComponentBase {
   public:
    static constexpr std::string_view kName = "search-index";

    SearchIndex(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    std::shared_ptr<const IndexSnapshot> Snapshot() const { return store_.Snapshot(); }
    std::shared_ptr<const IndexSnapshot> LoadUpload(std::string_view bytes, std::string name);
    const std::vector<std::string>& benchmark_queries() const { return benchmark_queries_; }
    const std::string& tokenizer_corpus_path() const { return tokenizer_corpus_path_; }
    const std::string& frontend() const { return frontend_; }

   private:
    static std::vector<std::string> LoadQueries(const std::string& path);
    static std::string LoadFrontend(const std::string& path);

    IndexStore store_;
    const std::vector<std::string> benchmark_queries_;
    const std::string tokenizer_corpus_path_;
    const std::string frontend_;
};

}  // namespace search_web

#endif  // WEBSEARCH_SEARCH_SERVICE_SRC_SEARCH_INDEX_HPP_
