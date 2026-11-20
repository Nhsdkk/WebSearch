#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "index_format.hpp"
#include "index_loader.hpp"
#include "index_saver.hpp"
#include "indexer.hpp"
#include "search_quality.hpp"
#include "test_files.hpp"
#include "zipf_analysis.hpp"

namespace {
using indexing::detail::IndexFormat;

class FrequencyTest : public ::testing::Test {
   protected:
    indexing::Index Build() {
        return indexing::Indexer{}.Build(files_.Write("cf.tsv", "A\turl/0\tb b b c\nA\turl/1\tb c c\n").string());
    }
    search_test::TestFiles files_;
};

TEST_F(FrequencyTest, CountsAllOccurrencesAndRoundTripsVersion12) {
    const auto index = Build();
    ASSERT_TRUE(index.frequencies());
    EXPECT_EQ(*index.frequencies(), (std::vector<std::uint64_t>{2, 4, 3}));
    EXPECT_EQ(index.Lookup("b"), (indexing::DocList{0, 1}));
    indexing::IndexSaver{}.Save(index, files_.Path("cf.bin").string(), 1);
    const auto loaded = indexing::IndexLoader{}.Load(files_.Path("cf.bin").string());
    EXPECT_EQ(loaded.frequencies(), index.frequencies());
    std::istringstream input(files_.Read("cf.bin"), std::ios::binary);
    IndexFormat::Header header;
    input >> header;
    EXPECT_EQ(header.minor_version, 2);
    EXPECT_EQ(header.section_count, 6);
}

TEST_F(FrequencyTest, KeepsLegacyIndexesSearchableWithoutInventingFrequencies) {
    const auto original = Build();
    const indexing::Index legacy(original.documents(), original.terms(), original.statistics());
    indexing::IndexSaver{}.Save(legacy, files_.Path("old.bin").string());
    const auto loaded = indexing::IndexLoader{}.Load(files_.Path("old.bin").string());
    EXPECT_FALSE(loaded.frequencies());
    EXPECT_FALSE(analysis::ZipfAnalysis{}.Run(loaded).available);
    EXPECT_EQ(boolsearch::BooleanSearch(loaded).Search("b"), (indexing::DocList{0, 1}));
    indexing::IndexSaver{}.Save(legacy, files_.Path("old11.bin").string(), 1);
    const auto timed = indexing::IndexLoader{}.Load(files_.Path("old11.bin").string());
    EXPECT_FALSE(timed.frequencies());
    EXPECT_TRUE(timed.statistics().indexing);
}

TEST_F(FrequencyTest, RejectsCorruptCollectionFrequencyPayloadsAndMetadata) {
    indexing::IndexSaver{}.Save(Build(), files_.Path("cf.bin").string());
    const auto original = files_.Read("cf.bin");
    std::istringstream input(original, std::ios::binary);
    IndexFormat::Header header;
    input >> header;
    IndexFormat::Section frequencies;
    for (std::uint32_t i = 0; i < header.section_count; ++i) {
        IndexFormat::Section section;
        input >> section;
        if (section.type == 6) {
            frequencies = section;
        }
    }
    ASSERT_EQ(frequencies.count, 3);
    for (const std::uint64_t count : {0, 1, 3, 10}) {
        std::stringstream bad(original, std::ios::binary | std::ios::in | std::ios::out);
        bad.seekp(static_cast<std::streamoff>(frequencies.offset));
        bad << IndexFormat::FrequencyRecord{count};
        files_.Write("bad.bin", bad.str());
        EXPECT_THROW(indexing::IndexLoader{}.Load(files_.Path("bad.bin").string()), std::runtime_error);
    }
    std::stringstream old_header(original, std::ios::binary | std::ios::in | std::ios::out);
    header.minor_version = 1;
    old_header << header;
    files_.Write("bad.bin", old_header.str());
    EXPECT_THROW(indexing::IndexLoader{}.Load(files_.Path("bad.bin").string()), std::runtime_error);
}

TEST(FrequencyRecordTest, OwnsBinaryOperatorsAndValidation) {
    std::ostringstream out(std::ios::binary);
    out << IndexFormat::FrequencyRecord{4};
    EXPECT_EQ(out.str(), std::string("\x04\0\0\0\0\0\0\0", 8));
    std::istringstream input(out.str(), std::ios::binary);
    IndexFormat::FrequencyRecord value;
    input >> value;
    EXPECT_EQ(value.count, 4);
    EXPECT_NO_THROW(value.Validate(2, 9));
    EXPECT_THROW(value.Validate(5, 9), std::runtime_error);
    EXPECT_THROW(value.Validate(2, 3), std::runtime_error);
    EXPECT_THROW(IndexFormat::FrequencyRecord::ValidateTotal(4, 5), std::runtime_error);
    std::istringstream short_input(std::string(7, '\0'));
    EXPECT_THROW(short_input >> value, std::runtime_error);
}

class ZipfTest : public ::testing::Test {
   protected:
    indexing::Index WithCounts(std::vector<std::uint64_t> counts) {
        std::vector<indexing::Term> terms;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            terms.push_back({std::to_string(i), {0}});
        }
        return indexing::Index({{"Title", "url"}}, std::move(terms), {}, std::move(counts));
    }
};

TEST_F(ZipfTest, FitsKnownZipfAndMandelbrotDistributions) {
    const auto zipf = analysis::ZipfAnalysis{}.Run(WithCounts({12, 6, 4, 3}));
    ASSERT_TRUE(zipf.zipf);
    EXPECT_NEAR(zipf.zipf->c, 12, 1e-10);
    EXPECT_NEAR(zipf.zipf->log_rmse, 0, 1e-10);
    const auto shifted = analysis::ZipfAnalysis{}.Run(WithCounts({60, 40, 30, 24, 20}));
    ASSERT_TRUE(shifted.mandelbrot);
    EXPECT_NEAR(shifted.mandelbrot->q, 1, 1e-4);
    EXPECT_NEAR(shifted.mandelbrot->s, 1, 1e-4);
    EXPECT_NEAR(shifted.mandelbrot->c, 120, 0.02);
    EXPECT_LT(shifted.mandelbrot->log_rmse, shifted.zipf->log_rmse);
}

TEST_F(ZipfTest, HandlesEmptySmallAndTiedDistributions) {
    const auto empty = analysis::ZipfAnalysis{}.Run(WithCounts({}));
    EXPECT_TRUE(empty.available);
    EXPECT_FALSE(empty.zipf);
    EXPECT_TRUE(empty.SampleRanks().empty());
    const auto one = analysis::ZipfAnalysis{}.Run(WithCounts({7}));
    EXPECT_DOUBLE_EQ(one.zipf->c, 7);
    EXPECT_FALSE(one.mandelbrot);
    const auto tied = analysis::ZipfAnalysis{}.Run(WithCounts({2, 4, 4}));
    EXPECT_EQ(tied.terms[0].term, 1);
    EXPECT_EQ(tied.terms[1].term, 2);
    EXPECT_EQ(tied.SampleRanks(2), (std::vector<std::size_t>{0, 2}));
}

TEST_F(ZipfTest, SamplesActualRanksAndPreservesEndpoints) {
    const auto report = analysis::ZipfAnalysis{}.Run(WithCounts(std::vector<std::uint64_t>(3000, 1)));
    const auto ranks = report.SampleRanks();
    EXPECT_LE(ranks.size(), 2000);
    ASSERT_FALSE(ranks.empty());
    EXPECT_EQ(ranks.front(), 0);
    EXPECT_EQ(ranks.back(), 2999);
    for (std::size_t i = 1; i < ranks.size(); ++i) {
        EXPECT_LT(ranks[i - 1], ranks[i]);
    }
    EXPECT_TRUE(report.SampleRanks(0).empty());
}

class QualityTest : public ::testing::Test {
   protected:
    void SetUp() override {
        index_ = indexing::Indexer{}.Build(
            files_.Write("quality.tsv", "A\turl/0\tмашина\nB\turl/1\tмашины\nC\turl/2\tмашиной\n").string());
        dataset_.index_sha256 = "fixture";
        dataset_.cases.push_back(
            {"q1",
             "машина",
             "Vehicles",
             "inflection",
             {{0, "url/0", true, "vehicle"}, {1, "url/1", true, "vehicle"}, {2, "url/2", false, "other intent"}}});
    }
    search_test::TestFiles files_;
    indexing::Index index_;
    analysis::SearchQualityBenchmark::Dataset dataset_;
};

TEST_F(QualityTest, CalculatesPrecisionAndRecallWithinJudgedPool) {
    const boolsearch::BooleanSearch search(index_);
    const auto results = analysis::SearchQualityBenchmark{}.Run(index_, search, dataset_, "fixture", 1);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].pool_relevant, 2);
    EXPECT_EQ(results[0].exact.total, 1);
    EXPECT_EQ(results[0].stem.total, 3);
    EXPECT_DOUBLE_EQ(results[0].exact.precision, 0.05);
    EXPECT_DOUBLE_EQ(results[0].stem.precision, 0.1);
    EXPECT_DOUBLE_EQ(results[0].exact.pool_recall, 0.5);
    EXPECT_DOUBLE_EQ(results[0].stem.pool_recall, 1);
}

TEST_F(QualityTest, RejectsWrongCorpusMissingJudgmentsAndInvalidRepetitions) {
    const boolsearch::BooleanSearch search(index_);
    const analysis::SearchQualityBenchmark benchmark;
    EXPECT_THROW(benchmark.Run(index_, search, dataset_, "other", 1), std::invalid_argument);
    EXPECT_THROW(benchmark.Run(index_, search, dataset_, "fixture", 0), std::invalid_argument);
    dataset_.cases[0].judgments.pop_back();
    EXPECT_THROW(benchmark.Run(index_, search, dataset_, "fixture", 1), std::invalid_argument);
    dataset_.cases[0].judgments[0].url = "wrong";
    EXPECT_THROW(dataset_.Validate(index_, "fixture"), std::invalid_argument);
}

TEST_F(QualityTest, DefinesZeroRelevantPoolsAndRejectsDuplicateJudgments) {
    for (auto& judgment : dataset_.cases[0].judgments) {
        judgment.relevant = false;
    }
    const boolsearch::BooleanSearch search(index_);
    const auto results = analysis::SearchQualityBenchmark{}.Run(index_, search, dataset_, "fixture", 1);
    EXPECT_DOUBLE_EQ(results[0].exact.pool_recall, 0);
    EXPECT_DOUBLE_EQ(results[0].stem.precision, 0);
    dataset_.cases[0].judgments.push_back(dataset_.cases[0].judgments[0]);
    EXPECT_THROW(dataset_.Validate(index_, "fixture"), std::invalid_argument);
}
}  // namespace
