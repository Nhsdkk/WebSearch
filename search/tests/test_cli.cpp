#include <gtest/gtest.h>

#include <ios>
#include <regex>
#include <sstream>
#include <string>

#include "boolean_search/cli.hpp"
#include "index_saver.hpp"
#include "indexer.hpp"
#include "test_files.hpp"

namespace {

class SearchCliTest : public ::testing::Test {
   protected:
    void SetUp() override {
        const auto corpus = files_.Write("corpus.tsv",
                                         "Кот\\t\"А\"\\nстрока\thttps://example.test/0\tcat\n"
                                         "Пёс\thttps://example.test/1\tdog\n");
        indexing::IndexSaver{}.Save(indexing::Indexer{}.Build(corpus.string()), files_.Path("index.bin").string());
    }

    int Run(const std::string& queries, boolsearch::MatchMode mode = boolsearch::MatchMode::kStem) {
        std::istringstream input(queries);
        return boolsearch::SearchCli{}.Run(files_.Path("index.bin").string(), input, output_, errors_, mode);
    }

    std::string Results() const {
        return std::regex_replace(output_.str(), std::regex(R"(,"elapsed_ms":[0-9]+\.[0-9]+)"), ",\"elapsed_ms\":0");
    }

    search_test::TestFiles files_;
    std::ostringstream output_;
    std::ostringstream errors_;
};

TEST_F(SearchCliTest, ProcessesCrLfBlankLinesAndFinalLineWithoutNewline) {
    EXPECT_EQ(Run("cat\r\n\r\ndog"), 0);
    EXPECT_EQ(Results(),
              "{\"query\":\"cat\",\"total\":1,\"elapsed_ms\":0,\"results\":["
              "{\"id\":0,\"title\":\"Кот\\u0009\\\"А\\\"\\u000aстрока\","
              "\"url\":\"https://example.test/0\"}],\"mode\":\"stem\"}\n"
              "{\"query\":\"\",\"total\":0,\"elapsed_ms\":0,\"results\":[],\"mode\":\"stem\"}\n"
              "{\"query\":\"dog\",\"total\":1,\"elapsed_ms\":0,\"results\":["
              "{\"id\":1,\"title\":\"Пёс\",\"url\":\"https://example.test/1\"}],\"mode\":\"stem\"}\n");
    EXPECT_TRUE(errors_.str().empty());
}

TEST_F(SearchCliTest, ContinuesAfterQueryErrors) {
    EXPECT_EQ(Run("cat ||\ndog\n"), 2);
    EXPECT_NE(output_.str().find("{\"query\":\"cat ||\",\"error\":"), std::string::npos);
    EXPECT_NE(output_.str().find("{\"query\":\"dog\",\"total\":1"), std::string::npos);
    EXPECT_TRUE(errors_.str().empty());
}

TEST_F(SearchCliTest, ReportsInvalidUtf8AsValidJson) {
    EXPECT_EQ(Run(std::string("bad\xff\ncat\n")), 2);
    EXPECT_NE(output_.str().find("{\"query\":null,\"error\":"), std::string::npos);
    EXPECT_EQ(output_.str().find('\xff'), std::string::npos);
    EXPECT_NE(output_.str().find("{\"query\":\"cat\",\"total\":1"), std::string::npos);
}

TEST_F(SearchCliTest, WritesAllMatches) {
    std::string corpus;
    for (int i = 0; i < 56; ++i) {
        corpus += "Title\thttps://example.test/" + std::to_string(i) + "\tcat\n";
    }
    indexing::IndexSaver{}.Save(indexing::Indexer{}.Build(files_.Write("corpus.tsv", corpus).string()),
                                files_.Path("index.bin").string());
    EXPECT_EQ(Run("cat\n"), 0);
    EXPECT_NE(output_.str().find("\"total\":56"), std::string::npos);
    EXPECT_NE(output_.str().find("\"id\":55"), std::string::npos);
}

TEST_F(SearchCliTest, HandlesEmptyInput) {
    EXPECT_EQ(Run(""), 0);
    EXPECT_TRUE(output_.str().empty());
    EXPECT_TRUE(errors_.str().empty());
}

TEST_F(SearchCliTest, ReportsStartupAndStreamFailures) {
    std::istringstream input("cat\n");
    const boolsearch::SearchCli cli;
    EXPECT_EQ(cli.Run(files_.Path("missing.bin").string(), input, output_, errors_), 1);
    EXPECT_TRUE(output_.str().empty());
    EXPECT_FALSE(errors_.str().empty());
    input.setstate(std::ios::badbit);
    EXPECT_EQ(cli.Run(files_.Path("index.bin").string(), input, output_, errors_), 1);
    output_.setstate(std::ios::badbit);
    EXPECT_EQ(Run("cat\n"), 1);
}

TEST_F(SearchCliTest, DefaultsToStemmingAndCanSelectExactForm) {
    ASSERT_EQ(Run("коты\n"), 0);
    EXPECT_NE(output_.str().find("\"total\":1"), std::string::npos);
    EXPECT_NE(output_.str().find("\"mode\":\"stem\""), std::string::npos);
    output_.str("");
    ASSERT_EQ(Run("коты\n", boolsearch::MatchMode::kExact), 0);
    EXPECT_NE(output_.str().find("\"total\":0"), std::string::npos);
    EXPECT_NE(output_.str().find("\"mode\":\"exact\""), std::string::npos);
}

}  // namespace
