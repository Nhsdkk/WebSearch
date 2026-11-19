#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "indexer.hpp"
#include "test_files.hpp"
#include "tokenization_benchmark.hpp"

namespace {
using indexing::Indexer;
using indexing::TokenizationBenchmark;
using search_test::TestFiles;

class TokenizationBenchmarkTest : public ::testing::Test {
   protected:
    TestFiles files_;
};

TEST_F(TokenizationBenchmarkTest, MeasuresRoundedPrefixesAndMatchesIndexCounters) {
    std::string corpus;
    for (int i = 0; i < 101; ++i) {
        corpus += "МАИ\thttps://example.test/" + std::to_string(i) + "\tЁж C++ café\n";
    }
    const auto path = files_.Write("corpus.tsv", corpus);
    const auto report = TokenizationBenchmark{}.Run(path.string(), 1);
    EXPECT_EQ(report.total_documents, 101);
    EXPECT_EQ(report.repetitions, 1);
    ASSERT_EQ(report.samples.size(), 3);
    const std::uint64_t expected_documents[] = {2, 11, 101};
    const std::uint32_t expected_percentages[] = {1, 10, 100};
    for (std::size_t i = 0; i < report.samples.size(); ++i) {
        const auto& sample = report.samples[i];
        const auto documents = expected_documents[i];
        EXPECT_EQ(sample.percentage, expected_percentages[i]);
        EXPECT_EQ(sample.documents, documents);
        EXPECT_EQ(sample.text_bytes, documents * 20);
        EXPECT_EQ(sample.tokens, documents * 4);
        EXPECT_EQ(sample.token_characters, documents * 9);
        EXPECT_EQ(sample.token_bytes, documents * 14);
        EXPECT_DOUBLE_EQ(sample.AverageTokenCharacters(), 2.25);
        EXPECT_DOUBLE_EQ(sample.AverageTokenBytes(), 3.5);
        ASSERT_EQ(sample.runs.size(), 1);
        EXPECT_GE(sample.runs.front().tokenization_ms, 0);
        EXPECT_GE(sample.runs.front().total_ms, sample.runs.front().tokenization_ms);
    }
    const auto index = Indexer{}.Build(path.string());
    const auto& full = report.samples.back();
    EXPECT_EQ(full.documents, index.documents().size());
    EXPECT_EQ(full.text_bytes, index.statistics().text_bytes);
    EXPECT_EQ(full.tokens, index.statistics().token_count);
    EXPECT_EQ(full.token_characters, index.statistics().token_characters);
}

TEST_F(TokenizationBenchmarkTest, DefaultsToThreeMeasuredRepetitionsAfterWarmup) {
    const auto path = files_.Write("corpus.tsv", "Title\thttps://example.test\tText\\ntext\n");
    const auto report = TokenizationBenchmark{}.Run(path.string());
    EXPECT_EQ(report.repetitions, 3);
    for (const auto& sample : report.samples) {
        EXPECT_EQ(sample.documents, 1);
        EXPECT_EQ(sample.text_bytes, 14);
        EXPECT_EQ(sample.tokens, 3);
        EXPECT_EQ(sample.runs.size(), 3);
    }
}

TEST_F(TokenizationBenchmarkTest, HandlesEmptyCorpusAndTextWithoutTokens) {
    const auto empty = files_.Write("empty.tsv", "");
    const auto report = TokenizationBenchmark{}.Run(empty.string(), 1);
    EXPECT_EQ(report.total_documents, 0);
    ASSERT_EQ(report.samples.size(), 3);
    for (const auto& sample : report.samples) {
        EXPECT_EQ(sample.documents, 0);
        EXPECT_EQ(sample.text_bytes, 0);
        EXPECT_EQ(sample.tokens, 0);
        EXPECT_DOUBLE_EQ(sample.AverageTokenCharacters(), 0);
        EXPECT_DOUBLE_EQ(sample.AverageTokenBytes(), 0);
        EXPECT_DOUBLE_EQ(sample.MillisecondsPerKib(), 0);
        EXPECT_DOUBLE_EQ(sample.KibPerSecond(), 0);
        EXPECT_EQ(sample.runs.size(), 1);
    }
    const auto punctuation = files_.Write("punctuation.tsv", "!!!\thttps://example.test\t---\n");
    const auto sample = TokenizationBenchmark{}.Run(punctuation.string(), 1).samples.back();
    EXPECT_EQ(sample.documents, 1);
    EXPECT_EQ(sample.text_bytes, 6);
    EXPECT_EQ(sample.tokens, 0);
    EXPECT_DOUBLE_EQ(sample.AverageTokenCharacters(), 0);
    EXPECT_DOUBLE_EQ(sample.AverageTokenBytes(), 0);
}

TEST(TokenizationBenchmarkMetricsTest, NormalizesByDecodedKibAndMeasuredRuns) {
    TokenizationBenchmark::Sample sample;
    EXPECT_DOUBLE_EQ(sample.MeanTokenizationMs(), 0);
    EXPECT_DOUBLE_EQ(sample.MeanTotalMs(), 0);
    EXPECT_DOUBLE_EQ(sample.MillisecondsPerKib(), 0);
    EXPECT_DOUBLE_EQ(sample.KibPerSecond(), 0);
    sample.text_bytes = 2048;
    EXPECT_DOUBLE_EQ(sample.KibPerSecond(), 0);
    sample.tokens = 2;
    sample.token_characters = 15;
    sample.token_bytes = 24;
    sample.runs = {{8, 12}, {12, 18}};
    EXPECT_DOUBLE_EQ(sample.MeanTokenizationMs(), 10);
    EXPECT_DOUBLE_EQ(sample.MeanTotalMs(), 15);
    EXPECT_DOUBLE_EQ(sample.MillisecondsPerKib(), 5);
    EXPECT_DOUBLE_EQ(sample.KibPerSecond(), 200);
    EXPECT_DOUBLE_EQ(sample.AverageTokenCharacters(), 7.5);
    EXPECT_DOUBLE_EQ(sample.AverageTokenBytes(), 12);
}

TEST_F(TokenizationBenchmarkTest, RejectsInvalidRepetitionsAndMalformedInput) {
    const auto path = files_.Write("corpus.tsv", "Title\thttps://example.test\tText\n");
    EXPECT_THROW(TokenizationBenchmark{}.Run(path.string(), 0), std::invalid_argument);
    EXPECT_THROW(TokenizationBenchmark{}.Run(path.string(), 11), std::invalid_argument);
    EXPECT_THROW(TokenizationBenchmark{}.Run(files_.Path("missing.tsv").string()), std::runtime_error);
    for (const auto& line : {std::string("invalid"), std::string("Title\turl\tbad\\q"),
                             std::string("Title\turl\tbad\xff"), std::string("bad\xff\turl\ttext")}) {
        const auto invalid = files_.Write("invalid.tsv", "\n" + line + "\n");
        try {
            TokenizationBenchmark{}.Run(invalid.string(), 1);
            FAIL() << "Expected a malformed corpus error";
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("Corpus line 2:"), std::string::npos);
        }
    }
}

}  // namespace
