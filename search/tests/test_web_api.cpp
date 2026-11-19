#include <curl/curl.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/formats/yaml/serialize.hpp>
#include <userver/formats/yaml/value_builder.hpp>
#include <utility>
#include <vector>

#include "test_files.hpp"
#include "test_process.hpp"

namespace {
using search_test::TestFiles;
using search_test::TestProcess;
namespace json = userver::formats::json;
namespace yaml = userver::formats::yaml;
using namespace std::chrono_literals;

class HttpClient {
   public:
    struct Response {
        long status = 0;
        std::string body;

        json::Value Json() const { return json::FromString(body); }
    };

    explicit HttpClient(std::string socket) : socket_(std::move(socket)) {
        Check(curl_global_init(CURL_GLOBAL_DEFAULT));
    }

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    ~HttpClient() { curl_global_cleanup(); }

    Response Request(const std::string& path, std::optional<std::string_view> payload = std::nullopt,
                     std::string_view content_type = "application/json") const {
        auto request = MakeHandle();
        const std::string url = "http://localhost" + path;
        Response response;
        const auto receive = +[](char* data, std::size_t size, std::size_t count, void* out) -> std::size_t {
            try {
                static_cast<std::string*>(out)->append(data, size * count);
                return size * count;
            } catch (...) {
                return 0;
            }
        };
        Check(curl_easy_setopt(request.get(), CURLOPT_URL, url.c_str()));
        Check(curl_easy_setopt(request.get(), CURLOPT_UNIX_SOCKET_PATH, socket_.c_str()));
        Check(curl_easy_setopt(request.get(), CURLOPT_NOPROXY, "*"));
        Check(curl_easy_setopt(request.get(), CURLOPT_TIMEOUT_MS, 2000L));
        Check(curl_easy_setopt(request.get(), CURLOPT_NOSIGNAL, 1L));
        Check(curl_easy_setopt(request.get(), CURLOPT_WRITEFUNCTION, receive));
        Check(curl_easy_setopt(request.get(), CURLOPT_WRITEDATA, &response.body));
        std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
        if (payload) {
            const auto header = "Content-Type: " + std::string(content_type);
            headers.reset(curl_slist_append(nullptr, header.c_str()));
            if (!headers) {
                throw std::bad_alloc();
            }
            Check(curl_easy_setopt(request.get(), CURLOPT_HTTPHEADER, headers.get()));
            Check(
                curl_easy_setopt(request.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload->size())));
            Check(curl_easy_setopt(request.get(), CURLOPT_POSTFIELDS, payload->data()));
        }

        const auto error = curl_easy_perform(request.get());
        if (error == CURLE_COULDNT_CONNECT) {
            return response;
        }
        Check(error);
        Check(curl_easy_getinfo(request.get(), CURLINFO_RESPONSE_CODE, &response.status));
        return response;
    }

    Response Search(const std::string& query, std::string_view page = "1") const {
        return Request("/api/search?q=" + Encode(query) + "&page=" + std::string(page));
    }

    Response Upload(std::string_view bytes, const std::string& name = "uploaded.bin") const {
        return Request("/api/index/load?filename=" + Encode(name), bytes, "application/octet-stream");
    }

   private:
    static std::string Encode(const std::string& text) {
        auto request = MakeHandle();
        std::unique_ptr<char, decltype(&curl_free)> encoded(curl_easy_escape(request.get(), text.c_str(), 0), curl_free);
        if (!encoded) {
            throw std::bad_alloc();
        }
        return encoded.get();
    }

    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

    static Handle MakeHandle() {
        Handle handle(curl_easy_init(), curl_easy_cleanup);
        if (!handle) {
            throw std::runtime_error("Cannot initialize curl request");
        }
        return handle;
    }

    static void Check(CURLcode code) {
        if (code != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(code));
        }
    }

    std::string socket_;
};

class WebApiTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::string corpus;
        for (int i = 0; i < 55; ++i) {
            const auto id = std::to_string(i);
            const auto title = i == 0 ? "<Первая>" : "Документ " + id;
            corpus += title + "\thttps://example.test/" + id + "\tмашина\n";
        }
        corpus += "Велосипед\thttps://example.test/55\tсиний велосипед\n";
        const auto input = files_.Write("corpus.tsv", corpus);
        const auto index = files_.Path("corpus.bin");
        TestProcess indexer({SEARCH_INDEXER_EXECUTABLE, input.string(), index.string()}, files_.Path("indexer.log"));
        ASSERT_EQ(indexer.WaitFor(5s), 0) << files_.Read("indexer.log");

        const std::string source = SEARCH_SOURCE_DIR;
        yaml::ValueBuilder config(yaml::blocking::FromFile(source + "/service/static_config.yaml"));
        auto components = config["components_manager"]["components"];
        components["handler-load-index"]["max_request_size"] = 16384;
        auto listener = components["server"]["listener"];
        for (const auto* key : {"address", "address#env", "address#fallback", "port", "port#env", "port#fallback"}) {
            listener.Remove(key);
        }
        listener["unix-socket"] = files_.Path("http").string();
        auto search_index = components["search-index"];
        search_index.Remove("index-path#env");
        search_index.Remove("index-path#fallback");
        search_index["index-path"] = index.string();
        search_index.Remove("tokenizer-corpus-path#env");
        search_index.Remove("tokenizer-corpus-path#fallback");
        search_index["tokenizer-corpus-path"] = input.string();
        search_index["benchmark-queries-path"] = files_.Write("queries.txt", "машина\n!машина\n").string();
        search_index["frontend-path"] = source + "/service/frontend/index.html";

        const auto config_path = files_.Write("config.yaml", yaml::ToString(config.ExtractValue()));
        server_.emplace(std::vector<std::string>{SEARCH_SERVICE_EXECUTABLE, "-c", config_path.string()},
                        files_.Path("server.log"));

        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (const auto status = server_->WaitFor(0ms)) {
                FAIL() << "Service exited with status " << *status << '\n' << files_.Read("server.log");
            }
            if (http_.Request("/api/stats").status == 200) {
                return;
            }
            std::this_thread::sleep_for(50ms);
        }
        FAIL() << "Service did not become ready\n" << files_.Read("server.log");
    }

    void TearDown() override {
        server_.reset();
        if (HasFailure() && std::filesystem::exists(files_.Path("server.log"))) {
            std::cerr << files_.Read("server.log");
        }
    }

    TestFiles files_;
    HttpClient http_{files_.Path("http").string()};
    std::optional<TestProcess> server_;
};

TEST_F(WebApiTest, ServesFrontendAndIndexStatistics) {
    const auto page = http_.Request("/");
    ASSERT_EQ(page.status, 200);
    EXPECT_NE(page.body.find("id=\"search-form\""), std::string::npos);
    EXPECT_NE(page.body.find("id=\"benchmark-form\""), std::string::npos);
    EXPECT_NE(page.body.find("id=\"index-form\""), std::string::npos);

    const auto response = http_.Request("/api/stats");
    ASSERT_EQ(response.status, 200);
    const auto stats = response.Json();
    EXPECT_EQ(stats["documents"].As<int>(), 56);
    EXPECT_EQ(stats["index_name"].As<std::string>(), "corpus.bin");
    EXPECT_GT(stats["terms"].As<int>(), 1);
    EXPECT_GE(stats["load_ms"].As<double>(), 0);
    EXPECT_GT(stats["average_token_characters"].As<double>(), 0);
    EXPECT_GT(stats["average_term_characters"].As<double>(), 0);
    const auto seconds = stats["total_seconds"].As<double>();
    EXPECT_GT(seconds, 0);
    EXPECT_GE(stats["build_ms"].As<double>(), 0);
    EXPECT_GE(stats["write_ms"].As<double>(), 0);
    EXPECT_NEAR(seconds * 1000, stats["build_ms"].As<double>() + stats["write_ms"].As<double>(), 1e-9);
    EXPECT_NEAR(stats["ms_per_document"].As<double>(), seconds * 1000 / 56, 1e-9);
    EXPECT_NEAR(stats["ms_per_kib"].As<double>(), seconds * 1000 / (stats["text_bytes"].As<double>() / 1024), 1e-9);
    const auto printed = json::FromString(files_.Read("indexer.log"));
    for (const auto* key : {"terms", "average_term_characters", "build_ms", "write_ms", "total_seconds",
                            "ms_per_document", "ms_per_kib", "documents_per_second", "kib_per_second"}) {
        SCOPED_TRACE(key);
        EXPECT_NEAR(stats[key].As<double>(), printed[key].As<double>(), 1e-6);
    }
    EXPECT_EQ(http_.Request("/missing").status, 404);
}

TEST_F(WebApiTest, PaginatesSearchAndReturnsMetadata) {
    const auto response = http_.Search("машина !синий");
    ASSERT_EQ(response.status, 200);
    const auto first = response.Json();
    EXPECT_EQ(first["total"].As<int>(), 55);
    EXPECT_EQ(first["total_pages"].As<int>(), 2);
    ASSERT_EQ(first["results"].GetSize(), 50);
    const auto result = first["results"][0];
    EXPECT_EQ(result.GetSize(), 3);
    EXPECT_EQ(result["id"].As<int>(), 0);
    EXPECT_EQ(result["title"].As<std::string>(), "<Первая>");
    EXPECT_EQ(result["url"].As<std::string>(), "https://example.test/0");

    const auto next = http_.Search("машина !синий", "2");
    ASSERT_EQ(next.status, 200);
    const auto results = next.Json()["results"];
    ASSERT_EQ(results.GetSize(), 5);
    for (std::size_t i = 0; i < results.GetSize(); ++i) {
        EXPECT_EQ(results[i]["id"].As<std::size_t>(), 50 + i);
    }

    const auto last = http_.Search("машина", std::to_string(std::numeric_limits<std::uint64_t>::max()));
    ASSERT_EQ(last.status, 200);
    EXPECT_EQ(last.Json()["results"].GetSize(), 0);
}

TEST_F(WebApiTest, EvaluatesQueriesAndRejectsInvalidInput) {
    const auto either = http_.Search("(машина || велосипед)");
    ASSERT_EQ(either.status, 200);
    EXPECT_EQ(either.Json()["total"].As<int>(), 56);
    const auto uppercase = http_.Search("МАШИНА");
    ASSERT_EQ(uppercase.status, 200);
    EXPECT_EQ(uppercase.Json()["total"].As<int>(), 55);
    EXPECT_EQ(http_.Search("машина ||").status, 400);
    EXPECT_EQ(http_.Search("машина", "0").status, 400);
    EXPECT_EQ(http_.Search("машина", "18446744073709551616").status, 400);
}

TEST_F(WebApiTest, BenchmarksDefaultAndCustomQueries) {
    const auto options = http_.Request("/api/benchmark");
    ASSERT_EQ(options.status, 200);
    const std::vector<std::string> queries{"машина", "!машина"};
    EXPECT_EQ(options.Json()["queries"].As<std::vector<std::string>>(), queries);

    const auto response = http_.Request("/api/benchmark", R"({"repetitions":3})");
    ASSERT_EQ(response.status, 200);
    const auto run = response.Json();
    EXPECT_EQ(run["repetitions"].As<int>(), 3);
    EXPECT_EQ(run["timed_executions"].As<int>(), 6);
    ASSERT_EQ(run["queries"].GetSize(), 2);
    EXPECT_EQ(run["queries"][0]["hits"].As<int>(), 55);
    EXPECT_EQ(run["queries"][1]["hits"].As<int>(), 1);
    for (const auto& row : run["queries"]) {
        const auto minimum = row["min_ms"].As<double>();
        const auto mean = row["mean_ms"].As<double>();
        const auto maximum = row["max_ms"].As<double>();
        EXPECT_GE(minimum, 0);
        EXPECT_LE(minimum, mean + 1e-9);
        EXPECT_LE(mean, maximum + 1e-9);
    }

    const auto custom = http_.Request("/api/benchmark", R"({"queries":["велосипед"],"repetitions":1})");
    ASSERT_EQ(custom.status, 200);
    EXPECT_EQ(custom.Json()["queries"][0]["hits"].As<int>(), 1);
}

TEST_F(WebApiTest, RejectsInvalidBenchmarkPayloads) {
    for (const auto* payload : {R"({"repetitions":0})", R"({"repetitions":101})", R"({"repetitions":1.5})",
                                R"({"queries":[]})", R"({"queries":[42]})", R"({"queries":["машина ||"]})"}) {
        SCOPED_TRACE(payload);
        const auto response = http_.Request("/api/benchmark", payload);
        ASSERT_EQ(response.status, 400);
        EXPECT_TRUE(response.Json().HasMember("error"));
    }
    EXPECT_EQ(http_.Request("/api/benchmark", "{bad json").status, 400);
}

TEST_F(WebApiTest, BenchmarksConfiguredTokenizerCorpus) {
    const auto options = http_.Request("/api/benchmark/tokenizer");
    ASSERT_EQ(options.status, 200);
    EXPECT_EQ(options.Json()["corpus"].As<std::string>(), "corpus.tsv");
    EXPECT_EQ(options.Json()["repetitions"].As<int>(), 3);
    EXPECT_EQ(options.Json()["max_repetitions"].As<int>(), 10);

    const auto response = http_.Request("/api/benchmark/tokenizer", "{}");
    ASSERT_EQ(response.status, 200);
    const auto report = response.Json();
    EXPECT_EQ(report["corpus"].As<std::string>(), "corpus.tsv");
    EXPECT_EQ(report["total_documents"].As<int>(), 56);
    EXPECT_EQ(report["repetitions"].As<int>(), 3);
    EXPECT_EQ(report["warmup_per_sample"].As<int>(), 1);
    const auto samples = report["samples"];
    ASSERT_EQ(samples.GetSize(), 3);
    EXPECT_EQ(samples[0]["documents"].As<int>(), 1);
    EXPECT_EQ(samples[1]["documents"].As<int>(), 6);
    EXPECT_EQ(samples[2]["documents"].As<int>(), 56);
    const auto stats = http_.Request("/api/stats").Json();
    EXPECT_EQ(samples[2]["tokens"].As<int>(), stats["tokens"].As<int>());
    EXPECT_EQ(samples[2]["text_bytes"].As<int>(), stats["text_bytes"].As<int>());
    EXPECT_DOUBLE_EQ(samples[2]["average_token_characters"].As<double>(),
                     stats["average_token_characters"].As<double>());
    for (const auto& sample : samples) {
        EXPECT_EQ(sample["runs"].GetSize(), 3);
        const auto milliseconds = sample["tokenization_mean_ms"].As<double>();
        const auto kib = sample["text_bytes"].As<double>() / 1024;
        EXPECT_GE(milliseconds, 0);
        EXPECT_GE(sample["total_mean_ms"].As<double>(), milliseconds);
        EXPECT_NEAR(sample["ms_per_kib"].As<double>(), milliseconds / kib, 1e-12);
        if (milliseconds > 0) {
            EXPECT_NEAR(sample["kib_per_second"].As<double>(), kib / (milliseconds / 1000), 1e-6);
        }
        EXPECT_DOUBLE_EQ(sample["average_token_bytes"].As<double>(),
                         sample["token_bytes"].As<double>() / sample["tokens"].As<double>());
    }
    const auto custom = http_.Request("/api/benchmark/tokenizer", R"({"repetitions":1})");
    ASSERT_EQ(custom.status, 200);
    EXPECT_EQ(custom.Json()["samples"][2]["runs"].GetSize(), 1);
}

TEST_F(WebApiTest, RejectsInvalidTokenizerBenchmarkPayloads) {
    for (const auto* payload : {"{bad json", "[]", R"({"repetitions":0})", R"({"repetitions":11})",
                                R"({"repetitions":1.5})", R"({"repetitions":-1})"}) {
        SCOPED_TRACE(payload);
        const auto response = http_.Request("/api/benchmark/tokenizer", payload);
        ASSERT_EQ(response.status, 400);
        EXPECT_TRUE(response.Json().HasMember("error"));
    }
}

TEST_F(WebApiTest, ServesConcurrentQueries) {
    // Four callers each issue three requests, as in the previous HTTP suite.
    std::vector<std::future<std::vector<HttpClient::Response>>> callers;
    for (int i = 0; i < 4; ++i) {
        callers.push_back(std::async(std::launch::async, [this] {
            std::vector<HttpClient::Response> responses;
            for (int j = 0; j < 3; ++j) {
                responses.push_back(http_.Search("машина"));
            }
            return responses;
        }));
    }
    for (auto& caller : callers) {
        for (const auto& response : caller.get()) {
            ASSERT_EQ(response.status, 200);
            EXPECT_EQ(response.Json()["total"].As<int>(), 55);
        }
    }
}

TEST_F(WebApiTest, UploadSwitchesSearchStatisticsAndBenchmarks) {
    const auto corpus = files_.Write("replacement.tsv",
                                     "Новая машина\thttps://example.test/new\tмашина\n"
                                     "Новый велосипед\thttps://example.test/bike\tвелосипед\n");
    TestProcess indexer({SEARCH_INDEXER_EXECUTABLE, corpus.string(), files_.Path("replacement.bin").string()},
                        files_.Path("replacement.log"));
    ASSERT_EQ(indexer.WaitFor(5s), 0) << files_.Read("replacement.log");
    const auto bytes = files_.Read("replacement.bin");
    const auto response = http_.Upload(bytes, "Мой индекс.bin");
    ASSERT_EQ(response.status, 200);
    EXPECT_EQ(response.Json()["index_name"].As<std::string>(), "Мой индекс.bin");
    EXPECT_EQ(response.Json()["documents"].As<int>(), 2);
    EXPECT_GT(response.Json()["total_seconds"].As<double>(), 0);
    const auto search = http_.Search("машина").Json();
    EXPECT_EQ(search["index_name"].As<std::string>(), "Мой индекс.bin");
    EXPECT_EQ(search["total"].As<int>(), 1);
    ASSERT_EQ(search["results"].GetSize(), 1);
    EXPECT_EQ(search["results"][0]["title"].As<std::string>(), "Новая машина");
    EXPECT_EQ(search["results"][0]["url"].As<std::string>(), "https://example.test/new");
    EXPECT_EQ(http_.Request("/api/stats").Json()["index_name"].As<std::string>(), "Мой индекс.bin");
    const auto run = http_.Request("/api/benchmark", R"({"repetitions":1})");
    ASSERT_EQ(run.status, 200);
    EXPECT_EQ(run.Json()["index_name"].As<std::string>(), "Мой индекс.bin");
    EXPECT_EQ(run.Json()["documents"].As<int>(), 2);
    EXPECT_EQ(run.Json()["queries"][0]["hits"].As<int>(), 1);
}

TEST_F(WebApiTest, InvalidUploadsPreserveActiveIndex) {
    for (const auto& bytes : {std::string{}, std::string{"invalid"}, files_.Read("corpus.bin").substr(0, 64)}) {
        const auto response = http_.Upload(bytes);
        ASSERT_EQ(response.status, 400);
        EXPECT_TRUE(response.Json().HasMember("error"));
        EXPECT_EQ(http_.Search("машина").Json()["total"].As<int>(), 55);
        EXPECT_EQ(http_.Request("/api/stats").Json()["index_name"].As<std::string>(), "corpus.bin");
    }
    EXPECT_EQ(http_.Request("/api/index/load", "{}").status, 415);
}

TEST_F(WebApiTest, UploadSizeLimitPreservesActiveIndex) {
    EXPECT_EQ(http_.Upload(std::string(20000, 'x')).status, 413);
    EXPECT_EQ(http_.Search("машина").Json()["total"].As<int>(), 55);
}

TEST_F(WebApiTest, AcceptsIndexWithoutTimings) {
    std::ifstream input(std::string(SEARCH_SOURCE_DIR) + "/data/demo.bin", std::ios::binary);
    ASSERT_TRUE(input);
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    // Convert the optional timing section to an unknown section. Its payload is
    // ignored by readers, just as in a version 1.0 index.
    ASSERT_GE(bytes.size(), 224);
    bytes[64 + 4 * 32] = 99;
    const auto response = http_.Upload(bytes, "legacy.bin");
    ASSERT_EQ(response.status, 200);
    EXPECT_TRUE(response.Json()["total_seconds"].IsNull());
    EXPECT_EQ(response.Json()["documents"].As<int>(), 5);
    EXPECT_EQ(http_.Search("!несуществующийтерм").Json()["total"].As<int>(), 5);
}

}  // namespace
