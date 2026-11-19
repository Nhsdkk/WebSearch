#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_FORMAT_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_FORMAT_HPP_

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace indexing {
class Tokenizer;
}

namespace indexing::detail {
// BIDX records serialize their fields explicitly; C++ padding is never stored.
// Document and term extraction require a seekable stream bounded by the section.
class IndexFormat {
   public:
    struct Header {
        static constexpr std::uint64_t kSize = 64;
        static constexpr std::uint32_t kSectionCount = 4;

        std::uint16_t major_version = 1;
        std::uint16_t minor_version = 0;
        std::uint32_t size = kSize;
        std::uint32_t section_count = kSectionCount;
        std::uint32_t flags = 0;
        std::uint64_t document_count = 0;
        std::uint64_t term_count = 0;
        std::uint64_t file_size = 0;
        std::uint64_t directory_offset = kSize;
        std::uint64_t reserved = 0;

        void Validate(std::uint64_t actual_file_size) const;

        friend std::istream& operator>>(std::istream& input, Header& record);
        friend std::ostream& operator<<(std::ostream& output, const Header& record);
    };

    struct Section {
        static constexpr std::uint64_t kSize = 32;

        std::uint32_t type = 0;
        std::uint32_t flags = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::uint64_t count = 0;

        // The header must already be validated against the file size.
        void Validate(const Header& header) const;
        // Call after validating the bounds of both sections.
        void ValidateAgainst(const Section& previous) const;

        friend std::istream& operator>>(std::istream& input, Section& record);
        friend std::ostream& operator<<(std::ostream& output, const Section& record);
    };

    struct DocumentRecord {
        static constexpr std::uint64_t kPrefixSize = 8;

        std::string title;
        std::string url;

        void Validate(const Tokenizer& tokenizer) const;
        static std::uint64_t SizeOf(const std::string& title, const std::string& url);

        friend std::istream& operator>>(std::istream& input, DocumentRecord& record);
        friend std::ostream& operator<<(std::ostream& output, const DocumentRecord& record);
    };

    struct TermRecord {
        static constexpr std::uint64_t kPrefixSize = 24;

        std::string text;
        std::uint32_t flags = 0;
        std::uint64_t first_posting = 0;
        std::uint64_t posting_count = 0;

        // Pass an empty previous term for the first dictionary entry.
        void Validate(const Tokenizer& tokenizer, std::string_view previous_term, std::uint64_t expected_first,
                      std::uint64_t total_postings, std::uint64_t document_count) const;
        static std::uint64_t SizeOf(const std::string& text);

        friend std::istream& operator>>(std::istream& input, TermRecord& record);
        friend std::ostream& operator<<(std::ostream& output, const TermRecord& record);
    };

    struct PostingRecord {
        static constexpr std::uint64_t kSize = 4;

        std::uint32_t document_id = 0;

        void Validate(std::uint64_t document_count, std::optional<std::uint32_t> previous_id) const;

        friend std::istream& operator>>(std::istream& input, PostingRecord& record);
        friend std::ostream& operator<<(std::ostream& output, const PostingRecord& record);
    };

    struct StatisticsRecord {
        static constexpr std::uint64_t kSize = 32;

        std::uint64_t text_bytes = 0;
        std::uint64_t token_count = 0;
        std::uint64_t token_characters = 0;
        std::uint64_t term_characters = 0;

        void Validate(std::uint64_t expected_term_characters, std::uint64_t total_postings) const;

        friend std::istream& operator>>(std::istream& input, StatisticsRecord& record);
        friend std::ostream& operator<<(std::ostream& output, const StatisticsRecord& record);
    };

    struct IndexingRecord {
        static constexpr std::uint64_t kSize = 16;

        std::uint64_t build_nanoseconds = 0;
        std::uint64_t write_nanoseconds = 0;

        void Validate() const;

        friend std::istream& operator>>(std::istream& input, IndexingRecord& record);
        friend std::ostream& operator<<(std::ostream& output, const IndexingRecord& record);
    };

    static constexpr unsigned char kMagic[8] = {'B', 'I', 'D', 'X', '\r', '\n', 0x1a, '\n'};

    // Finds a required section or throws if it is absent.
    static Section FindSection(const std::vector<Section>& sections, std::uint32_t type);
};
}  // namespace indexing::detail

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_INDEX_FORMAT_HPP_
