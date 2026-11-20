#include "search_index.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/yaml_config/merge_schemas.hpp>
#include <utility>
#include <vector>

namespace search_web {
SearchIndex::SearchIndex(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      store_(config["index-path"].As<std::string>()),
      benchmark_queries_(LoadQueries(config["benchmark-queries-path"].As<std::string>())),
      tokenizer_corpus_path_(config["tokenizer-corpus-path"].As<std::string>("data/demo.tsv")),
      quality_dataset_path_(config["quality-dataset-path"].As<std::string>("data/relevance-judgments.json")),
      frontend_(LoadFrontend(config["frontend-path"].As<std::string>())) {}

userver::yaml_config::Schema SearchIndex::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Active BIDX index and frontend files
additionalProperties: false
properties:
    index-path:
        type: string
        description: Path to the binary index
    benchmark-queries-path:
        type: string
        description: Path to the default benchmark queries, one per line
    tokenizer-corpus-path:
        type: string
        description: Escaped TSV corpus used by the tokenization benchmark
    frontend-path:
        type: string
        description: Path to the frontend HTML file
    quality-dataset-path:
        type: string
        description: Judged top-20 pools bound to a binary index SHA-256
)");
}

std::shared_ptr<const IndexSnapshot> SearchIndex::LoadUpload(std::string_view bytes, std::string name) {
    return store_.LoadUpload(bytes, std::move(name));
}

std::vector<std::string> SearchIndex::LoadQueries(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open benchmark queries: " + path);
    }

    std::vector<std::string> queries;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            queries.push_back(line);
        }
    }
    if (!input.eof() || queries.empty() || queries.size() > 20) {
        throw std::runtime_error("Benchmark query file must contain 1 to 20 queries: " + path);
    }
    return queries;
}

std::string SearchIndex::LoadFrontend(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open frontend: " + path);
    }
    return {std::istreambuf_iterator<char>(input), {}};
}
}  // namespace search_web
