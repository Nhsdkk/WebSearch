#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "boolean_search/query.hpp"
#include "indexer.hpp"
#include "russian_stemmer.hpp"
#include "test_files.hpp"

namespace {
TEST(RussianStemmerTest, FollowsReferenceRulesAndLongestSuffixes) {
    const std::pair<const char*, const char*> cases[] = {{"вагонами", "вагон"},
                                                         {"важнейшими", "важн"},
                                                         {"важностью", "важност"},
                                                         {"величие", "велич"},
                                                         {"бегавшая", "бега"},
                                                         {"машины", "машин"},
                                                         {"машиной", "машин"},
                                                         {"институтами", "институт"},
                                                         {"авиационного", "авиацион"},
                                                         {"работавшись", "работа"},
                                                         {"падающего", "пада"},
                                                         {"валялось", "валя"},
                                                         {"новости", "новост"},
                                                         {"ёлками", "елк"},
                                                         {"ёлка", "елк"},
                                                         {"сильнейший", "сильн"},
                                                         {"сталь", "стал"},
                                                         {"стали", "стал"},
                                                         {"стал", "стал"},
                                                         {"павел", "павел"},
                                                         {"павла", "павл"},
                                                         {"я", "я"},
                                                         {"в", "в"},
                                                         {"съезд", "съезд"},
                                                         {"а", "а"},
                                                         {"", ""}};
    for (const auto& sample : cases) {
        SCOPED_TRACE(sample.first);
        EXPECT_EQ(morphology::RussianStemmer{}.Stem(sample.first), sample.second);
    }
}

TEST(RussianStemmerTest, LeavesNonRussianTokensUnchangedAndRejectsInvalidUtf8) {
    for (const auto* token : {"running", "2026", "москва123", "testмосква", "café"}) {
        EXPECT_EQ(morphology::RussianStemmer{}.Stem(token), token);
    }
    EXPECT_THROW(morphology::RussianStemmer{}.Stem("bad\xff"), std::runtime_error);
}

TEST(RussianStemmerTest, MatchesOfficialRussianReferenceVocabulary) {
    std::ifstream input(SEARCH_RUSSIAN_REFERENCE);
    ASSERT_TRUE(input);
    const morphology::RussianStemmer stemmer;
    std::size_t checked = 0;
    for (std::string line; std::getline(input, line);) {
        const auto tab = line.find('\t');
        ASSERT_NE(tab, std::string::npos);
        const auto word = line.substr(0, tab);
        EXPECT_EQ(stemmer.Stem(word), line.substr(tab + 1)) << word;
        ++checked;
    }
    EXPECT_TRUE(input.eof());
    EXPECT_GT(checked, 40000);
}

class StemmingSearchTest : public ::testing::Test {
   protected:
    indexing::Index Build() {
        return indexing::Indexer{}.Build(files_
                                             .Write("forms.tsv",
                                                    "A\thttps://test/0\tмашина самолёт\n"
                                                    "B\thttps://test/1\tмашины самолётом\n"
                                                    "C\thttps://test/2\tмашиной поезд\n"
                                                    "D\thttps://test/3\tелка ёлки\n")
                                             .string());
    }
    search_test::TestFiles files_;
};

TEST_F(StemmingSearchTest, DefaultsToWordFormsAndPreservesExactMode) {
    const auto index = Build();
    const boolsearch::BooleanSearch search(index);
    EXPECT_EQ(search.Search("МАШИНА"), (indexing::DocList{0, 1, 2}));
    EXPECT_EQ(search.Search("машина", boolsearch::MatchMode::kExact), (indexing::DocList{0}));
    EXPECT_EQ(search.Search("самолет"), (indexing::DocList{0, 1}));
    EXPECT_EQ(search.Search("самолет", boolsearch::MatchMode::kExact), indexing::DocList{});
    EXPECT_EQ(search.Search("ёлка"), (indexing::DocList{3}));
}

TEST_F(StemmingSearchTest, AppliesFormsInsideBooleanOperatorsAndNegation) {
    const auto index = Build();
    const boolsearch::BooleanSearch search(index);
    const std::pair<const char*, indexing::DocList> cases[] = {
        {"машина самолет", {0, 1}}, {"машина !самолет", {2}}, {"!машина", {3}}, {"(машина || елка) !самолет", {2, 3}},
        {"!!машинами", {0, 1, 2}},  {"неизвестный", {}}};
    for (const auto& sample : cases) {
        EXPECT_EQ(search.Search(sample.first), sample.second);
    }
    EXPECT_EQ(search.Search("!машина", boolsearch::MatchMode::kExact), (indexing::DocList{1, 2, 3}));
}

TEST(MatchModeTest, AcceptsOnlyExplicitModes) {
    EXPECT_EQ(boolsearch::MatchModes::Parse("stem"), boolsearch::MatchMode::kStem);
    EXPECT_EQ(boolsearch::MatchModes::Parse("exact"), boolsearch::MatchMode::kExact);
    EXPECT_THROW(boolsearch::MatchModes::Parse("other"), std::invalid_argument);
}
}  // namespace
