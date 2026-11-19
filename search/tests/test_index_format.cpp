#include <gtest/gtest.h>

#include <cstddef>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "index_format.hpp"

namespace {
using indexing::detail::IndexFormat;

template <class Record>
Record CheckRecord(const Record& record, const std::vector<unsigned char>& bytes) {
    const std::string encoded(bytes.begin(), bytes.end());
    std::ostringstream output(std::ios::binary);
    output << record;
    EXPECT_EQ(output.str(), encoded);

    std::istringstream input(encoded, std::ios::binary);
    Record decoded;
    input >> decoded;
    EXPECT_EQ(input.peek(), std::char_traits<char>::eof());

    for (std::size_t end = 0; end < bytes.size(); ++end) {
        SCOPED_TRACE(end);
        std::istringstream truncated(encoded.substr(0, end), std::ios::binary);
        Record incomplete;
        EXPECT_THROW(truncated >> incomplete, std::runtime_error);
    }
    return decoded;
}

TEST(IndexFormatTest, Header) {
    IndexFormat::Header header;
    header.document_count = 3;
    header.term_count = 7;
    header.file_size = 512;
    const auto decoded = CheckRecord(
        header, {
                    0x42, 0x49, 0x44, 0x58, 0x0d, 0x0a, 0x1a, 0x0a, 1, 0, 0, 0, 64, 0, 0, 0, 4, 0, 0, 0, 0, 0,
                    0,    0,    3,    0,    0,    0,    0,    0,    0, 0, 7, 0, 0,  0, 0, 0, 0, 0, 0, 2, 0, 0,
                    0,    0,    0,    0,    64,   0,    0,    0,    0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0,
                });
    EXPECT_EQ(decoded.major_version, 1);
    EXPECT_EQ(decoded.minor_version, 0);
    EXPECT_EQ(decoded.size, 64);
    EXPECT_EQ(decoded.section_count, 4);
    EXPECT_EQ(decoded.flags, 0);
    EXPECT_EQ(decoded.document_count, 3);
    EXPECT_EQ(decoded.term_count, 7);
    EXPECT_EQ(decoded.file_size, 512);
    EXPECT_EQ(decoded.directory_offset, 64);
    EXPECT_EQ(decoded.reserved, 0);
}

TEST(IndexFormatTest, Section) {
    const IndexFormat::Section section{2, 0, 192, 64, 3};
    const auto decoded = CheckRecord(
        section, {2, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(decoded.type, 2);
    EXPECT_EQ(decoded.flags, 0);
    EXPECT_EQ(decoded.offset, 192);
    EXPECT_EQ(decoded.size, 64);
    EXPECT_EQ(decoded.count, 3);
}

TEST(IndexFormatTest, Document) {
    const IndexFormat::DocumentRecord document{"Ёж", "u"};
    const auto decoded = CheckRecord(document, {4, 0, 0, 0, 1, 0, 0, 0, 0xd0, 0x81, 0xd0, 0xb6, 0x75});
    EXPECT_EQ(decoded.title, "Ёж");
    EXPECT_EQ(decoded.url, "u");
    EXPECT_EQ(IndexFormat::DocumentRecord::SizeOf(document.title, document.url), 13);
}

TEST(IndexFormatTest, Term) {
    const IndexFormat::TermRecord term{"ёж", 0, 2, 3};
    const auto decoded = CheckRecord(
        term, {4, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0xd1, 0x91, 0xd0, 0xb6});
    EXPECT_EQ(decoded.text, "ёж");
    EXPECT_EQ(decoded.flags, 0);
    EXPECT_EQ(decoded.first_posting, 2);
    EXPECT_EQ(decoded.posting_count, 3);
    EXPECT_EQ(IndexFormat::TermRecord::SizeOf(term.text), 28);
}

TEST(IndexFormatTest, Posting) {
    const auto decoded = CheckRecord(IndexFormat::PostingRecord{0x12345678}, {0x78, 0x56, 0x34, 0x12});
    EXPECT_EQ(decoded.document_id, 0x12345678);
}

TEST(IndexFormatTest, Statistics) {
    const IndexFormat::StatisticsRecord statistics{0x0102030405060708ULL, 2, 9, 4};
    const auto decoded = CheckRecord(
        statistics, {8, 7, 6, 5, 4, 3, 2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(decoded.text_bytes, 0x0102030405060708ULL);
    EXPECT_EQ(decoded.token_count, 2);
    EXPECT_EQ(decoded.token_characters, 9);
    EXPECT_EQ(decoded.term_characters, 4);
}

TEST(IndexFormatTest, EmptyStringsReplacePreviousValues) {
    std::istringstream input(std::string(8, '\0'), std::ios::binary);
    IndexFormat::DocumentRecord document{"old title", "old URL"};
    input >> document;
    EXPECT_TRUE(document.title.empty());
    EXPECT_TRUE(document.url.empty());
}

TEST(IndexFormatTest, IndexingTimings) {
    const IndexFormat::IndexingRecord record{0x0102030405060708ULL, 2};
    const auto decoded = CheckRecord(record, {8, 7, 6, 5, 4, 3, 2, 1, 2, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(decoded.build_nanoseconds, record.build_nanoseconds);
    EXPECT_EQ(decoded.write_nanoseconds, 2);
    EXPECT_NO_THROW(decoded.Validate());

    const IndexFormat::IndexingRecord overflow{UINT64_MAX, 1};
    EXPECT_THROW(overflow.Validate(), std::runtime_error);
}
}  // namespace
