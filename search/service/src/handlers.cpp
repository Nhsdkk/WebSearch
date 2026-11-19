#include "handlers.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ratio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <userver/formats/json.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/http/content_type.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <utility>
#include <vector>

#include "index.hpp"
#include "search_index.hpp"
#include "tokenization_benchmark.hpp"

namespace search_web {
namespace {
using userver::formats::json::Type;
using userver::formats::json::ValueBuilder;
using userver::server::http::HttpRequest;
using userver::server::http::HttpStatus;

constexpr std::size_t kPageSize = 50;
constexpr std::uint64_t kDefaultRepetitions = 10;
constexpr std::uint64_t kMaxRepetitions = 100;
constexpr std::size_t kMaxBenchmarkQueries = 20;
constexpr std::size_t kMaxQueryBytes = 65536;

using Clock = std::chrono::steady_clock;

double ElapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

std::string JsonResponse(HttpRequest& request, ValueBuilder& body, HttpStatus status = HttpStatus::kOk) {
    auto& response = request.GetHttpResponse();
    response.SetContentType(userver::http::content_type::kApplicationJson);
    response.SetStatus(status);
    return userver::formats::json::ToString(body.ExtractValue());
}

std::string BadRequest(HttpRequest& request, const std::string& message) {
    ValueBuilder body(Type::kObject);
    body["error"] = message;
    return JsonResponse(request, body, HttpStatus::kBadRequest);
}

std::uint64_t ParsePage(const HttpRequest& request) {
    if (!request.HasArg("page")) {
        return 1;
    }
    const auto& argument = request.GetArg("page");
    std::uint64_t page = 0;
    const auto [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), page);
    if (error != std::errc{} || end != argument.data() + argument.size() || page == 0) {
        throw std::invalid_argument("Page must be a positive integer");
    }
    return page;
}

struct BenchmarkOptions {
    std::uint64_t repetitions = kDefaultRepetitions;
    std::vector<std::string> queries;
};

BenchmarkOptions ParseBenchmark(const HttpRequest& request, const std::vector<std::string>& defaults) {
    BenchmarkOptions options;
    options.queries = defaults;
    if (request.RequestBody().empty()) {
        return options;
    }
    const auto body = userver::formats::json::FromString(request.RequestBody());
    if (!body.IsObject()) {
        throw std::invalid_argument("Benchmark body must be a JSON object");
    }
    if (body.HasMember("repetitions")) {
        options.repetitions = body["repetitions"].As<std::uint64_t>();
    }
    if (options.repetitions == 0 || options.repetitions > kMaxRepetitions) {
        throw std::invalid_argument("Repetitions must be between 1 and 100");
    }
    if (body.HasMember("queries")) {
        const auto queries = body["queries"];
        if (!queries.IsArray() || queries.GetSize() == 0 || queries.GetSize() > kMaxBenchmarkQueries) {
            throw std::invalid_argument("Provide between 1 and 20 benchmark queries");
        }
        options.queries = queries.As<std::vector<std::string>>();
    }
    for (const auto& query : options.queries) {
        if (query.empty() || query.size() > kMaxQueryBytes) {
            throw std::invalid_argument("Benchmark queries must contain 1 to 65536 UTF-8 bytes");
        }
    }
    return options;
}

class Benchmark {
   public:
    static ValueBuilder Run(const IndexSnapshot& index, const BenchmarkOptions& options) {
        std::vector<std::size_t> hits;
        hits.reserve(options.queries.size());
        for (const auto& query : options.queries) {
            try {
                hits.push_back(index.Search(query).size());
            } catch (const std::runtime_error& error) {
                throw std::invalid_argument("Query " + std::to_string(hits.size() + 1) + ": " + error.what());
            }
        }

        double total_ms = 0;
        ValueBuilder queries(Type::kArray);
        for (std::size_t i = 0; i < options.queries.size(); ++i) {
            queries.PushBack(Measure(index, options.queries[i], hits[i], options.repetitions, total_ms));
        }
        const auto executions = options.queries.size() * options.repetitions;
        ValueBuilder report(Type::kObject);
        report["documents"] = index.index().documents().size();
        report["terms"] = index.index().terms().size();
        report["load_ms"] = index.load_ms();
        report["index_name"] = index.name();
        report["repetitions"] = options.repetitions;
        report["warmup_per_query"] = 1;
        report["query_count"] = options.queries.size();
        report["timed_executions"] = executions;
        report["total_search_ms"] = total_ms;
        report["overall_mean_ms"] = total_ms / static_cast<double>(executions);
        report["queries"] = std::move(queries);
        return report;
    }

   private:
    static ValueBuilder Measure(const IndexSnapshot& index, const std::string& query, std::size_t hits,
                                std::uint64_t repetitions, double& total_ms) {
        double minimum = 0;
        double maximum = 0;
        double total = 0;
        for (std::uint64_t i = 0; i < repetitions; ++i) {
            const auto start = Clock::now();
            const auto matches = index.Search(query);
            const double elapsed = ElapsedMs(start);
            if (matches.size() != hits) {
                throw std::logic_error("Inconsistent search result count");
            }
            minimum = i == 0 ? elapsed : std::min(minimum, elapsed);
            maximum = std::max(maximum, elapsed);
            total += elapsed;
        }
        total_ms += total;
        ValueBuilder result(Type::kObject);
        result["query"] = query;
        result["hits"] = hits;
        result["min_ms"] = minimum;
        result["mean_ms"] = total / static_cast<double>(repetitions);
        result["max_ms"] = maximum;
        return result;
    }
};

}  // namespace

IndexHandler::IndexHandler(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), search_index_(context.FindComponent<SearchIndex>()) {}

std::string SearchHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    const auto snapshot = search_index_.Snapshot();
    const auto& query = request.GetArg("q");
    if (query.size() > kMaxQueryBytes) {
        return BadRequest(request, "Query exceeds 65536 bytes");
    }
    std::uint64_t page = 0;
    indexing::DocList matches;
    double elapsed = 0;
    try {
        page = ParsePage(request);
        const auto start = Clock::now();
        matches = snapshot->Search(query);
        elapsed = ElapsedMs(start);
    } catch (const std::invalid_argument& error) {
        return BadRequest(request, error.what());
    } catch (const std::runtime_error& error) {
        return BadRequest(request, error.what());
    }

    const auto total_pages = matches.size() / kPageSize + (matches.size() % kPageSize != 0);
    const auto begin = page <= total_pages ? static_cast<std::size_t>(page - 1) * kPageSize : matches.size();
    const auto count = std::min(kPageSize, matches.size() - begin);
    ValueBuilder results(Type::kArray);
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = matches[begin + i];
        const auto& document = snapshot->index().documents()[id];
        ValueBuilder row(Type::kObject);
        row["id"] = id;
        row["title"] = document.title;
        row["url"] = document.url;
        results.PushBack(std::move(row));
    }

    ValueBuilder body(Type::kObject);
    body["query"] = query;
    body["index_name"] = snapshot->name();
    body["page"] = page;
    body["page_size"] = kPageSize;
    body["total"] = matches.size();
    body["total_pages"] = total_pages;
    body["elapsed_ms"] = elapsed;
    body["results"] = std::move(results);
    return JsonResponse(request, body);
}

ValueBuilder IndexHandler::Statistics(const IndexSnapshot& snapshot) {
    const auto& index = snapshot.index();
    const auto& stats = index.statistics();
    const auto term_count = static_cast<double>(index.terms().size());
    const auto token_count = static_cast<double>(stats.token_count);

    ValueBuilder body(Type::kObject);
    body["index_name"] = snapshot.name();
    body["documents"] = index.documents().size();
    body["terms"] = index.terms().size();
    body["index_bytes"] = snapshot.bytes();
    body["text_bytes"] = stats.text_bytes;
    body["tokens"] = stats.token_count;
    body["average_term_characters"] = term_count == 0 ? 0.0 : static_cast<double>(stats.term_characters) / term_count;
    body["average_token_characters"] =
        token_count == 0 ? 0.0 : static_cast<double>(stats.token_characters) / token_count;
    body["load_ms"] = snapshot.load_ms();
    body["build_ms"] = nullptr;
    body["write_ms"] = nullptr;
    body["total_seconds"] = nullptr;
    body["ms_per_document"] = nullptr;
    body["ms_per_kib"] = nullptr;
    body["documents_per_second"] = nullptr;
    body["kib_per_second"] = nullptr;
    if (stats.indexing) {
        const auto& timings = *stats.indexing;
        body["build_ms"] = static_cast<double>(timings.build_nanoseconds) / 1e6;
        body["write_ms"] = static_cast<double>(timings.write_nanoseconds) / 1e6;
        body["total_seconds"] = timings.TotalSeconds();
        body["ms_per_document"] = timings.MillisecondsPerDocument(index.documents().size());
        body["ms_per_kib"] = timings.MillisecondsPerKib(stats.text_bytes);
        body["documents_per_second"] = timings.DocumentsPerSecond(index.documents().size());
        body["kib_per_second"] = timings.KibPerSecond(stats.text_bytes);
    }
    return body;
}

std::string StatisticsHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    const auto snapshot = search_index_.Snapshot();
    auto body = Statistics(*snapshot);
    return JsonResponse(request, body);
}

std::string LoadIndexHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    if (request.GetHeader("Content-Type") != "application/octet-stream") {
        ValueBuilder error(Type::kObject);
        error["error"] = "Upload a BIDX file as application/octet-stream";
        return JsonResponse(request, error, HttpStatus::kUnsupportedMediaType);
    }
    try {
        auto name = request.GetArg("filename");
        if (name.empty()) {
            name = "uploaded.bin";
        }
        const auto snapshot = search_index_.LoadUpload(request.RequestBody(), std::move(name));
        auto body = Statistics(*snapshot);
        return JsonResponse(request, body);
    } catch (const std::invalid_argument& error) {
        return BadRequest(request, error.what());
    }
}

std::string BenchmarkHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    const auto& defaults = search_index_.benchmark_queries();
    if (request.GetMethod() == userver::server::http::HttpMethod::kGet ||
        request.GetMethod() == userver::server::http::HttpMethod::kHead) {
        ValueBuilder body(Type::kObject);
        ValueBuilder queries(Type::kArray);
        for (const auto& query : defaults) {
            queries.PushBack(ValueBuilder(query));
        }
        body["queries"] = std::move(queries);
        body["repetitions"] = kDefaultRepetitions;
        body["max_repetitions"] = kMaxRepetitions;
        return JsonResponse(request, body);
    }

    try {
        const auto snapshot = search_index_.Snapshot();
        auto report = Benchmark::Run(*snapshot, ParseBenchmark(request, defaults));
        return JsonResponse(request, report);
    } catch (const userver::formats::json::Exception& error) {
        return BadRequest(request, error.what());
    } catch (const std::invalid_argument& error) {
        return BadRequest(request, error.what());
    }
}

std::string TokenizationBenchmarkHandler::HandleRequest(HttpRequest& request,
                                                        userver::server::request::RequestContext&) const {
    using indexing::TokenizationBenchmark;
    const auto& path = search_index_.tokenizer_corpus_path();
    ValueBuilder body(Type::kObject);
    body["corpus"] = std::filesystem::path(path).filename().string();
    if (request.GetMethod() == userver::server::http::HttpMethod::kGet ||
        request.GetMethod() == userver::server::http::HttpMethod::kHead) {
        body["repetitions"] = TokenizationBenchmark::kDefaultRepetitions;
        body["max_repetitions"] = TokenizationBenchmark::kMaxRepetitions;
        return JsonResponse(request, body);
    }
    try {
        auto repetitions = TokenizationBenchmark::kDefaultRepetitions;
        if (!request.RequestBody().empty()) {
            const auto options = userver::formats::json::FromString(request.RequestBody());
            if (!options.IsObject()) {
                throw std::invalid_argument("Benchmark body must be a JSON object");
            }
            if (options.HasMember("repetitions")) {
                repetitions = options["repetitions"].As<std::uint32_t>();
            }
        }
        const auto report = TokenizationBenchmark{}.Run(path, repetitions);
        body["total_documents"] = report.total_documents;
        body["repetitions"] = report.repetitions;
        body["warmup_per_sample"] = 1;
        ValueBuilder samples(Type::kArray);
        for (const auto& sample : report.samples) {
            ValueBuilder row(Type::kObject);
            row["percentage"] = sample.percentage;
            row["documents"] = sample.documents;
            row["text_bytes"] = sample.text_bytes;
            row["tokens"] = sample.tokens;
            row["token_characters"] = sample.token_characters;
            row["token_bytes"] = sample.token_bytes;
            row["average_token_characters"] = sample.AverageTokenCharacters();
            row["average_token_bytes"] = sample.AverageTokenBytes();
            row["tokenization_mean_ms"] = sample.MeanTokenizationMs();
            row["total_mean_ms"] = sample.MeanTotalMs();
            row["ms_per_kib"] = sample.MillisecondsPerKib();
            row["kib_per_second"] = sample.KibPerSecond();
            ValueBuilder runs(Type::kArray);
            for (const auto& timing : sample.runs) {
                ValueBuilder run(Type::kObject);
                run["tokenization_ms"] = timing.tokenization_ms;
                run["total_ms"] = timing.total_ms;
                runs.PushBack(std::move(run));
            }
            row["runs"] = std::move(runs);
            samples.PushBack(std::move(row));
        }
        body["samples"] = std::move(samples);
        return JsonResponse(request, body);
    } catch (const userver::formats::json::Exception& error) {
        return BadRequest(request, error.what());
    } catch (const std::runtime_error& error) {
        return BadRequest(request, error.what());
    } catch (const std::invalid_argument& error) {
        return BadRequest(request, error.what());
    }
}

std::string FrontendHandler::HandleRequest(HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetContentType(userver::http::ContentType("text/html; charset=utf-8"));
    response.SetHeader(std::string_view("Content-Security-Policy"),
                       "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline' "
                       "https://fonts.googleapis.com; font-src https://fonts.gstatic.com; connect-src 'self'; "
                       "base-uri 'none'; frame-ancestors 'none'");
    return search_index_.frontend();
}

}  // namespace search_web
