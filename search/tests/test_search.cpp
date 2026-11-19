#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "boolean_search/query.hpp"
#include "index.hpp"
#include "index_loader.hpp"
#include "index_saver.hpp"
#include "indexer.hpp"
#include "test_files.hpp"
#include "tokenizer.hpp"

namespace {
using boolsearch::BooleanSearch;
using indexing::DocId;
using indexing::DocList;
using indexing::Document;
using indexing::Index;
using indexing::Indexer;
using indexing::IndexLoader;
using indexing::IndexSaver;
using indexing::Statistics;
using indexing::Term;
using indexing::Tokenizer;
using search_test::TestFiles;

static_assert(std::is_same_v<decltype(std::declval<Index&>().documents()), const std::vector<Document>&>);
static_assert(std::is_same_v<decltype(std::declval<Index&>().terms()), const std::vector<Term>&>);
static_assert(std::is_same_v<decltype(std::declval<Index&>().statistics()), const Statistics&>);
static_assert(!std::is_constructible_v<BooleanSearch, Index&&>);

class SearchTest : public ::testing::Test {
   protected:
    Index BuildIndex() {
        const auto corpus = files_.Write("fixture.tsv",
                                         "Zero\thttps://example.org/0\ta a b МОСКВА ЁЖ\n"
                                         "One\thttps://example.org/1\tb c Москва\n"
                                         "Two\thttps://example.org/2\tc d\n"
                                         "Three\thttps://example.org/3\t\n");
        return Indexer{}.Build(corpus.string());
    }

    TestFiles files_;
};

TEST(TokenizerTest, NormalizesLatinAndCyrillic) {
    const std::vector<std::string> expected{"москва", "ёж", "moscow123", "2026"};
    EXPECT_EQ(Tokenizer{}.Tokenize("МОСКВА, ЁЖ! Moscow123\n2026"), expected);
    EXPECT_EQ(Tokenizer{}.Utf8Length("Ёж abc"), 6);
}

TEST(TokenizerTest, RejectsMalformedUtf8) {
    EXPECT_THROW(Tokenizer{}.Tokenize(std::string("bad\xc0\xaf", 5)), std::runtime_error);
    EXPECT_THROW(Tokenizer{}.Tokenize(std::string("bad\xff", 4)), std::runtime_error);
}

TEST(TokenizerTest, PreservesCurrentAccentHyphenAndTechnicalNameRules) {
    const std::vector<std::string> expected{"caf", "cafe", "na", "ve", "санкт", "петербург", "c", "c"};
    EXPECT_EQ(Tokenizer{}.Tokenize("café cafe\u0301 naïve Санкт-Петербург C++ C#"), expected);
}

TEST_F(SearchTest, IndexesTitlesAndDeduplicatesPostings) {
    const auto index = BuildIndex();
    EXPECT_EQ(index.documents().size(), 4);
    EXPECT_EQ(index.Lookup("a"), DocList{0});
    EXPECT_TRUE(index.Lookup("missing").empty());

    const BooleanSearch search(index);
    EXPECT_EQ(search.Search("a"), DocList{0});
    EXPECT_EQ(search.Search("МОСКВА"), (DocList{0, 1}));
    EXPECT_EQ(search.Search("ЁЖ"), DocList{0});
    EXPECT_EQ(search.Search("zero"), DocList{0});
}

TEST_F(SearchTest, EvaluatesBooleanOperatorsAndPrecedence) {
    const auto index = BuildIndex();
    const BooleanSearch search(index);
    const std::pair<const char*, DocList> cases[] = {
        {"a b", {0}},
        {"  a\t\t&&\n b  ", {0}},
        {"b || c", {0, 1, 2}},
        {"a || b c", {0, 1}},
        {"(a || b) c", {1}},
        {"b !a", {1}},
        {"(a||b)(b||c)", {0, 1}},
        {"!a", {1, 2, 3}},
        {"!!a", {0}},
        {"!(a || c)", {3}},
        {"!missing", {0, 1, 2, 3}},
        {"a || !a", {0, 1, 2, 3}},
        {"a && !a", {}},
        {"", {}},
        {" \t\r\n ", {}},
    };
    for (const auto& [query, expected] : cases) {
        EXPECT_EQ(search.Search(query), expected) << "Query: " << query;
    }
}

TEST_F(SearchTest, RejectsMalformedQueries) {
    const auto index = BuildIndex();
    const BooleanSearch search(index);
    for (const auto* query : {"(", ")", "()", "a ||", "&& a", "!", "a || || b", "a & b", "a | b"}) {
        SCOPED_TRACE(query);
        EXPECT_THROW(search.Search(query), std::runtime_error);
    }
    EXPECT_THROW(search.Search(std::string(200, '(') + "a" + std::string(200, ')')), std::runtime_error);
}

TEST_F(SearchTest, RoundTripsIndexDeterministically) {
    const auto index = BuildIndex();
    const auto path = files_.Path("index.bin");
    IndexSaver{}.Save(index, path.string());
    const auto restored = IndexLoader{}.Load(path.string());

    ASSERT_EQ(restored.documents().size(), index.documents().size());
    for (std::size_t i = 0; i < index.documents().size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(restored.documents()[i].title, index.documents()[i].title);
        EXPECT_EQ(restored.documents()[i].url, index.documents()[i].url);
    }
    ASSERT_EQ(restored.terms().size(), index.terms().size());
    for (std::size_t i = 0; i < index.terms().size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(restored.terms()[i].text, index.terms()[i].text);
        EXPECT_EQ(restored.terms()[i].postings, index.terms()[i].postings);
    }
    EXPECT_EQ(restored.statistics().token_count, index.statistics().token_count);
    EXPECT_FALSE(restored.statistics().indexing.has_value());
    EXPECT_EQ(BooleanSearch(restored).Search("МОСКВА !ЁЖ"), DocList{1});

    IndexSaver{}.Save(BuildIndex(), files_.Path("again.bin").string());
    EXPECT_EQ(files_.Read("index.bin"), files_.Read("again.bin"));
}

TEST_F(SearchTest, PreservesIndexingMetricsWhenReloadingAndSaving) {
    const auto index = BuildIndex();
    const auto path = files_.Path("timed.bin");
    const auto saved = IndexSaver{}.Save(index, path.string(), 2000000000);
    ASSERT_TRUE(saved.has_value());
    const auto restored = IndexLoader{}.Load(path.string());
    const auto& timings = restored.statistics().indexing;
    ASSERT_TRUE(timings.has_value());
    EXPECT_EQ(timings->build_nanoseconds, 2000000000);
    EXPECT_EQ(timings->write_nanoseconds, saved->write_nanoseconds);
    EXPECT_GE(timings->TotalSeconds(), 2);
    EXPECT_DOUBLE_EQ(timings->MillisecondsPerDocument(4), timings->TotalSeconds() * 250);
    EXPECT_DOUBLE_EQ(timings->MillisecondsPerKib(2048), timings->TotalSeconds() * 500);
    EXPECT_DOUBLE_EQ(timings->DocumentsPerSecond(4), 4 / timings->TotalSeconds());
    EXPECT_DOUBLE_EQ(timings->KibPerSecond(2048), 2 / timings->TotalSeconds());
    EXPECT_DOUBLE_EQ(timings->MillisecondsPerDocument(0), 0);
    EXPECT_DOUBLE_EQ(timings->MillisecondsPerKib(0), 0);
    const indexing::IndexingTimings empty;
    EXPECT_DOUBLE_EQ(empty.DocumentsPerSecond(0), 0);
    EXPECT_DOUBLE_EQ(empty.KibPerSecond(0), 0);

    IndexSaver{}.Save(restored, files_.Path("again.bin").string());
    EXPECT_EQ(files_.Read("timed.bin"), files_.Read("again.bin"));
}

class IndexFileTest : public SearchTest {
   protected:
    void SetUp() override {
        IndexSaver{}.Save(BuildIndex(), files_.Path("index.bin").string());
        bytes_ = files_.Read("index.bin");
    }

    static std::uint64_t ReadLittleEndian(const std::string& bytes, std::size_t offset, unsigned width) {
        std::uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i) {
            result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes.at(offset + i))) << (8 * i);
        }
        return result;
    }

    static void WriteLittleEndian(std::string& bytes, std::size_t offset, std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i) {
            bytes.at(offset + i) = static_cast<char>(value >> (8 * i));
        }
    }

    std::string bytes_;
};

TEST_F(IndexFileTest, RejectsEveryTruncation) {
    for (std::size_t size = 0; size < bytes_.size(); ++size) {
        SCOPED_TRACE(size);
        const auto path = files_.Write("broken.bin", bytes_.substr(0, size));
        EXPECT_THROW(IndexLoader{}.Load(path.string()), std::runtime_error);
    }
}

TEST_F(IndexFileTest, RejectsInvalidSectionsAndRecords) {
    const auto docs_start = ReadLittleEndian(bytes_, 64 + 8, 8);
    const auto terms_start = ReadLittleEndian(bytes_, 64 + 32 + 8, 8);
    const auto posting_start = ReadLittleEndian(bytes_, 64 + 2 * 32 + 8, 8);
    const auto statistics_start = ReadLittleEndian(bytes_, 64 + 3 * 32 + 8, 8);
    struct Corruption {
        const char* description;
        std::size_t offset;
        std::uint64_t value;
        unsigned width;
    };
    const Corruption cases[] = {
        {"incorrect magic", 0, 0, 1},
        {"unsupported major version", 8, 2, 2},
        {"excessive section count", 16, UINT32_MAX, 4},
        {"unsupported section flags", 64 + 4, 1, 4},
        {"section offset overflow", 64 + 8, UINT64_MAX, 8},
        {"section size overflow", 64 + 16, UINT64_MAX, 8},
        {"section covering the directory", 64 + 8, 64, 8},
        {"duplicate section type", 64 + 32, 1, 4},
        {"inconsistent document count", 64 + 24, 1, 8},
        {"document string crossing a section", static_cast<std::size_t>(docs_start), posting_start - docs_start, 4},
        {"empty document title", static_cast<std::size_t>(docs_start), 0, 4},
        {"invalid document UTF-8", static_cast<std::size_t>(docs_start) + 8, 0xff, 1},
        {"overlapping sections", 64 + 32 + 8, docs_start, 8},
        {"unsupported term flags", static_cast<std::size_t>(terms_start) + 4, 1, 4},
        {"noncontiguous posting range", static_cast<std::size_t>(terms_start) + 8, 1, 8},
        {"non-normalized term", static_cast<std::size_t>(terms_start) + 24, 'A', 1},
        {"out-of-range posting ID", static_cast<std::size_t>(posting_start), 4, 4},
        // The dictionary starts with a:{0}, b:{0,1}; duplicate b's IDs.
        {"duplicate posting IDs", static_cast<std::size_t>(posting_start) + 8, 0, 4},
        {"inconsistent term statistics", static_cast<std::size_t>(statistics_start) + 24, 0, 8},
    };
    for (const auto& [description, offset, value, width] : cases) {
        SCOPED_TRACE(description);
        auto damaged = bytes_;
        WriteLittleEndian(damaged, offset, value, width);
        const auto path = files_.Write("broken.bin", damaged);
        EXPECT_THROW(IndexLoader{}.Load(path.string()), std::runtime_error);
    }
}

TEST_F(IndexFileTest, SkipsUnknownSectionInNewMinorVersion) {
    // Relocate the directory and append an unknown section.
    auto extended = bytes_;
    extended += bytes_.substr(64, 4 * 32);
    extended += std::string(32, '\0');
    const auto extension_entry = bytes_.size() + 4 * 32;
    WriteLittleEndian(extended, 10, 5, 2);
    WriteLittleEndian(extended, 16, 5, 4);
    WriteLittleEndian(extended, 48, bytes_.size(), 8);
    WriteLittleEndian(extended, extension_entry, 99, 4);
    WriteLittleEndian(extended, extension_entry + 8, extended.size(), 8);
    WriteLittleEndian(extended, extension_entry + 16, 1, 8);
    WriteLittleEndian(extended, extension_entry + 24, 1, 8);
    extended += 'x';
    WriteLittleEndian(extended, 40, extended.size(), 8);
    const auto path = files_.Write("extended.bin", extended);

    const auto future = IndexLoader{}.Load(path.string());
    EXPECT_EQ(future.documents().size(), 4);
    EXPECT_EQ(BooleanSearch(future).Search("МОСКВА !ЁЖ"), DocList{1});
}

TEST_F(SearchTest, DecodesAndPreservesTsvEscapes) {
    const auto corpus = files_.Write("fixture.tsv", "Escaped\\tTitle\\nLine\\\\End\thttps://example.org\tbody\\nnew\n");
    const auto index = Indexer{}.Build(corpus.string());
    ASSERT_EQ(index.documents().size(), 1);
    EXPECT_EQ(index.documents()[0].title, "Escaped\tTitle\nLine\\End");
    EXPECT_EQ(BooleanSearch(index).Search("body new"), DocList{0});

    const auto binary = files_.Path("index.bin");
    IndexSaver{}.Save(index, binary.string());
    const auto restored = IndexLoader{}.Load(binary.string());
    ASSERT_EQ(restored.documents().size(), 1);
    EXPECT_EQ(restored.documents()[0].title, index.documents()[0].title);
}

TEST_F(SearchTest, RejectsInvalidTsv) {
    const auto corpus = files_.Write("fixture.tsv", "title\turl\n");
    EXPECT_THROW(Indexer{}.Build(corpus.string()), std::runtime_error);
    files_.Write("fixture.tsv", "title\turl\tbad\\q\n");
    EXPECT_THROW(Indexer{}.Build(corpus.string()), std::runtime_error);
}

TEST_F(SearchTest, HandlesEmptyCorpus) {
    const auto index = Indexer{}.Build(files_.Write("fixture.tsv", "").string());
    EXPECT_TRUE(BooleanSearch(index).Search("!a").empty());
    const auto binary = files_.Path("index.bin");
    IndexSaver{}.Save(index, binary.string());
    EXPECT_TRUE(IndexLoader{}.Load(binary.string()).documents().empty());
}

// The oracle evaluates each document independently using its term membership.
class ExpressionGenerator {
   public:
    struct Expression {
        std::string query;
        std::vector<bool> values;
    };

    std::uint32_t Next() {
        seed_ = seed_ * 1664525U + 1013904223U;
        return seed_;
    }

    Expression Generate(unsigned depth, const std::vector<std::vector<bool>>& membership) {
        const auto doc_count = membership.front().size();
        const unsigned choice = Next() % (depth ? 4 : 1);
        if (choice == 0) {
            const auto term = Next() % membership.size();
            return {"term" + std::to_string(term), membership[term]};
        }
        auto left = Generate(depth - 1, membership);
        if (choice == 1) {
            for (std::size_t d = 0; d < doc_count; ++d) {
                left.values[d] = !left.values[d];
            }
            left.query = "!(" + left.query + ")";
            return left;
        }
        const auto right = Generate(depth - 1, membership);
        for (std::size_t d = 0; d < doc_count; ++d) {
            left.values[d] = choice == 2 ? left.values[d] && right.values[d] : left.values[d] || right.values[d];
        }
        const std::string op = choice == 2 ? (Next() % 2 ? " && " : " ") : " || ";
        left.query = "(" + left.query + op + right.query + ")";
        return left;
    }

   private:
    std::uint32_t seed_ = 0x1234abcdU;
};

TEST_F(SearchTest, MatchesDocumentByDocumentOracle) {
    constexpr std::size_t docs = 41;
    constexpr std::size_t terms = 9;
    ExpressionGenerator generator;
    std::vector<std::vector<bool>> membership(terms, std::vector<bool>(docs));
    std::string synthetic;
    for (std::size_t d = 0; d < docs; ++d) {
        synthetic += "Document" + std::to_string(d) + "\thttps://example.org/" + std::to_string(d) + "\t";
        for (std::size_t t = 0; t + 1 < terms; ++t) {
            membership[t][d] = (generator.Next() >> 16) % 3 != 0;
            if (membership[t][d]) {
                synthetic += "term" + std::to_string(t) + " ";
            }
        }
        synthetic += "\n";
    }
    const auto index = Indexer{}.Build(files_.Write("fixture.tsv", synthetic).string());
    const BooleanSearch search(index);
    for (unsigned trial = 0; trial < 500; ++trial) {
        const auto generated = generator.Generate(5, membership);
        DocList expected;
        for (std::size_t d = 0; d < docs; ++d) {
            if (generated.values[d]) {
                expected.push_back(static_cast<DocId>(d));
            }
        }
        EXPECT_EQ(search.Search(generated.query), expected) << "Trial " << trial << ": " << generated.query;
    }
}
}  // namespace
