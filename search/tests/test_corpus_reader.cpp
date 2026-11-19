#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "corpus_reader.hpp"
#include "test_files.hpp"

namespace {
using indexing::CorpusDocument;
using indexing::CorpusReader;
using search_test::TestFiles;

class CorpusReaderTest : public ::testing::Test {
   protected:
    TestFiles files_;
};

TEST_F(CorpusReaderTest, DecodesEscapesAndHandlesBomBlankLinesAndCrlf) {
    const auto path = files_.Write("corpus.tsv",
                                   "\xef\xbb\xbf\r\n"
                                   "\r\n"
                                   "Заг\\tоловок\thttps://example.test/a\\\\b\tA\\nB\\rC\\tD\\\\E\r\n"
                                   "Last\thttps://example.test/last\t");
    CorpusReader reader(path.string());
    CorpusDocument document;
    ASSERT_TRUE(reader.Next(document));
    EXPECT_EQ(reader.line_number(), 3);
    EXPECT_EQ(document.title, "Заг\tоловок");
    EXPECT_EQ(document.url, "https://example.test/a\\b");
    EXPECT_EQ(document.text, "A\nB\rC\tD\\E");
    ASSERT_TRUE(reader.Next(document));
    EXPECT_EQ(reader.line_number(), 4);
    EXPECT_EQ(document.title, "Last");
    EXPECT_EQ(document.url, "https://example.test/last");
    EXPECT_TRUE(document.text.empty());
    EXPECT_FALSE(reader.Next(document));
}

TEST_F(CorpusReaderTest, ReadsEmptyCorpusAndRejectsMissingFile) {
    const auto path = files_.Write("empty.tsv", "\n\r\n");
    CorpusReader reader(path.string());
    CorpusDocument document;
    EXPECT_FALSE(reader.Next(document));
    EXPECT_EQ(reader.line_number(), 2);
    EXPECT_THROW(CorpusReader{files_.Path("missing.tsv").string()}, std::runtime_error);
}

TEST_F(CorpusReaderTest, RejectsMalformedRecordsWithPhysicalLineNumber) {
    const std::string cases[] = {"one field",
                                 "title\turl",
                                 "title\turl\ttext\textra",
                                 "\turl\ttext",
                                 "title\t\ttext",
                                 "title\turl\tunknown\\q",
                                 "title\turl\ttrailing\\",
                                 "title\tinvalid\xff\ttext"};
    for (const auto& line : cases) {
        SCOPED_TRACE(line);
        const auto path = files_.Write("invalid.tsv", "\n" + line + "\n");
        CorpusReader reader(path.string());
        CorpusDocument document;
        try {
            reader.Next(document);
            FAIL() << "Expected a malformed corpus error";
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("Corpus line 2:"), std::string::npos);
        }
    }
}

}  // namespace
