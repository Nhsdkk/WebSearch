#include "index_format.hpp"

#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>
#include <ios>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "checked_arithmetic.hpp"
#include "tokenizer.hpp"

namespace indexing::detail {
namespace {

static_assert(CHAR_BIT == 8, "BIDX requires eight-bit bytes");
static_assert(std::endian::native == std::endian::little, "BIDX requires a little-endian host");

void ReadBytes(std::istream& input, char* data, std::streamsize size) {
    if (!input.read(data, size)) {
        throw std::runtime_error("Truncated index field");
    }
}

template <typename Uint>
void ReadUint(std::istream& input, Uint& value) {
    ReadBytes(input, reinterpret_cast<char*>(&value), sizeof(value));
}

template <typename Uint>
void WriteUint(std::ostream& output, Uint value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::uint32_t Length32(const std::string& value) {
    if (value.size() > CheckedArithmetic::kMaxUint32) {
        throw std::runtime_error("String exceeds binary format limit");
    }
    return static_cast<std::uint32_t>(value.size());
}

// Check the section's remaining bytes before allocating space for its strings.
void CheckTextSize(std::istream& input, std::uint64_t size) {
    const auto start = input.tellg();
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    input.seekg(start);
    if (!input || start < 0 || end < start || size > static_cast<std::uint64_t>(end - start)) {
        throw std::runtime_error("Truncated index string");
    }
}

void ReadText(std::istream& input, std::uint32_t length, std::string& value) {
    value.resize(length);
    ReadBytes(input, value.data(), length);
}

}  // namespace

void IndexFormat::Header::Validate(std::uint64_t actual_file_size) const {
    if (major_version != 1) {
        throw std::runtime_error("Unsupported index major version");
    }
    if (flags != 0) {
        throw std::runtime_error("Unsupported mandatory index flags");
    }

    // Higher minor versions may extend the header or add optional sections.
    constexpr std::uint32_t kMaxSectionCount = 1024;
    const bool valid_size = size >= kSize && size <= actual_file_size && file_size == actual_file_size;
    const bool valid_counts = document_count <= CheckedArithmetic::kMaxUint32 && section_count >= kSectionCount &&
                              section_count <= kMaxSectionCount;
    const bool valid_directory = directory_offset >= size && directory_offset <= file_size &&
                                 section_count <= (file_size - directory_offset) / Section::kSize;
    if (reserved != 0 || !valid_size || !valid_counts || !valid_directory) {
        throw std::runtime_error("Invalid index header");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::Header& header) {
    unsigned char magic[sizeof(IndexFormat::kMagic)];
    ReadBytes(input, reinterpret_cast<char*>(magic), sizeof(magic));
    if (std::memcmp(magic, IndexFormat::kMagic, sizeof(magic)) != 0) {
        throw std::runtime_error("Not a BIDX binary index");
    }

    ReadUint(input, header.major_version);
    ReadUint(input, header.minor_version);
    ReadUint(input, header.size);
    ReadUint(input, header.section_count);
    ReadUint(input, header.flags);

    ReadUint(input, header.document_count);
    ReadUint(input, header.term_count);
    ReadUint(input, header.file_size);
    ReadUint(input, header.directory_offset);
    ReadUint(input, header.reserved);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::Header& header) {
    output.write(reinterpret_cast<const char*>(IndexFormat::kMagic), sizeof(IndexFormat::kMagic));
    WriteUint(output, header.major_version);
    WriteUint(output, header.minor_version);
    WriteUint(output, header.size);
    WriteUint(output, header.section_count);
    WriteUint(output, header.flags);

    WriteUint(output, header.document_count);
    WriteUint(output, header.term_count);
    WriteUint(output, header.file_size);
    WriteUint(output, header.directory_offset);
    WriteUint(output, header.reserved);
    return output;
}

void IndexFormat::Section::Validate(const Header& header) const {
    const auto directory_end = header.directory_offset + header.section_count * kSize;
    const bool in_file = offset >= header.size && offset <= header.file_size && size <= header.file_size - offset;
    if (flags != 0 || !in_file || (size != 0 && offset < directory_end && offset + size > header.directory_offset)) {
        throw std::runtime_error("Invalid index section bounds or flags");
    }
}

void IndexFormat::Section::ValidateAgainst(const Section& previous) const {
    if (type == previous.type) {
        throw std::runtime_error("Duplicate index section type");
    }
    if (size != 0 && previous.size != 0 && offset < previous.offset + previous.size && previous.offset < offset + size) {
        throw std::runtime_error("Overlapping index sections");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::Section& section) {
    ReadUint(input, section.type);
    ReadUint(input, section.flags);
    ReadUint(input, section.offset);
    ReadUint(input, section.size);
    ReadUint(input, section.count);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::Section& section) {
    WriteUint(output, section.type);
    WriteUint(output, section.flags);
    WriteUint(output, section.offset);
    WriteUint(output, section.size);
    WriteUint(output, section.count);
    return output;
}

void IndexFormat::DocumentRecord::Validate(const Tokenizer& tokenizer) const {
    if (title.empty() || url.empty()) {
        throw std::runtime_error("Empty document metadata in index");
    }
    (void)tokenizer.Utf8Length(title);
    (void)tokenizer.Utf8Length(url);
}

std::uint64_t IndexFormat::DocumentRecord::SizeOf(const std::string& title, const std::string& url) {
    return kPrefixSize + Length32(title) + Length32(url);
}

std::istream& operator>>(std::istream& input, IndexFormat::DocumentRecord& document) {
    std::uint32_t title_length = 0;
    std::uint32_t url_length = 0;
    ReadUint(input, title_length);
    ReadUint(input, url_length);

    CheckTextSize(input, std::uint64_t{title_length} + url_length);
    ReadText(input, title_length, document.title);
    ReadText(input, url_length, document.url);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::DocumentRecord& document) {
    const auto title_length = Length32(document.title);
    const auto url_length = Length32(document.url);
    WriteUint(output, title_length);
    WriteUint(output, url_length);

    output.write(document.title.data(), title_length);
    output.write(document.url.data(), url_length);
    return output;
}

void IndexFormat::TermRecord::Validate(const Tokenizer& tokenizer, std::string_view previous_term,
                                       std::uint64_t expected_first, std::uint64_t total_postings,
                                       std::uint64_t document_count) const {
    if (flags != 0) {
        throw std::runtime_error("Unsupported term flags");
    }

    const auto normalized = tokenizer.Tokenize(text);
    const bool valid_range = first_posting == expected_first && first_posting <= total_postings && posting_count != 0 &&
                             posting_count <= total_postings - first_posting && posting_count <= document_count;
    if (normalized.size() != 1 || normalized[0] != text || previous_term >= text || !valid_range) {
        throw std::runtime_error("Invalid dictionary or posting range");
    }
}

std::uint64_t IndexFormat::TermRecord::SizeOf(const std::string& text) { return kPrefixSize + Length32(text); }

std::istream& operator>>(std::istream& input, IndexFormat::TermRecord& term) {
    std::uint32_t length = 0;
    ReadUint(input, length);
    ReadUint(input, term.flags);
    ReadUint(input, term.first_posting);
    ReadUint(input, term.posting_count);

    CheckTextSize(input, length);
    ReadText(input, length, term.text);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::TermRecord& term) {
    const auto length = Length32(term.text);
    WriteUint(output, length);
    WriteUint(output, term.flags);
    WriteUint(output, term.first_posting);
    WriteUint(output, term.posting_count);
    output.write(term.text.data(), length);
    return output;
}

void IndexFormat::PostingRecord::Validate(std::uint64_t document_count, std::optional<std::uint32_t> previous_id) const {
    if (document_id >= document_count || (previous_id && *previous_id >= document_id)) {
        throw std::runtime_error("Invalid or unsorted posting ID");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::PostingRecord& posting) {
    ReadUint(input, posting.document_id);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::PostingRecord& posting) {
    WriteUint(output, posting.document_id);
    return output;
}

void IndexFormat::StatisticsRecord::Validate(std::uint64_t expected_term_characters,
                                             std::uint64_t total_postings) const {
    if (term_characters != expected_term_characters || token_count < total_postings || token_characters < token_count ||
        (token_count == 0 && token_characters != 0)) {
        throw std::runtime_error("Inconsistent index statistics");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::StatisticsRecord& statistics) {
    ReadUint(input, statistics.text_bytes);
    ReadUint(input, statistics.token_count);
    ReadUint(input, statistics.token_characters);
    ReadUint(input, statistics.term_characters);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::StatisticsRecord& statistics) {
    WriteUint(output, statistics.text_bytes);
    WriteUint(output, statistics.token_count);
    WriteUint(output, statistics.token_characters);
    WriteUint(output, statistics.term_characters);
    return output;
}

void IndexFormat::IndexingRecord::Validate() const {
    if (build_nanoseconds > std::numeric_limits<std::uint64_t>::max() - write_nanoseconds) {
        throw std::runtime_error("Indexing time overflow");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::IndexingRecord& record) {
    ReadUint(input, record.build_nanoseconds);
    ReadUint(input, record.write_nanoseconds);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::IndexingRecord& record) {
    WriteUint(output, record.build_nanoseconds);
    WriteUint(output, record.write_nanoseconds);
    return output;
}

void IndexFormat::FrequencyRecord::Validate(std::uint64_t document_frequency, std::uint64_t token_count) const {
    if (count == 0 || count < document_frequency || count > token_count) {
        throw std::runtime_error("Invalid collection frequency");
    }
}

void IndexFormat::FrequencyRecord::ValidateTotal(std::uint64_t sum, std::uint64_t token_count) {
    if (sum != token_count) {
        throw std::runtime_error("Collection frequencies do not sum to token count");
    }
}

std::istream& operator>>(std::istream& input, IndexFormat::FrequencyRecord& record) {
    ReadUint(input, record.count);
    return input;
}

std::ostream& operator<<(std::ostream& output, const IndexFormat::FrequencyRecord& record) {
    WriteUint(output, record.count);
    return output;
}

IndexFormat::Section IndexFormat::FindSection(const std::vector<Section>& sections, std::uint32_t type) {
    for (const auto& section : sections) {
        if (section.type == type) {
            return section;
        }
    }
    throw std::runtime_error("Missing required index section");
}

}  // namespace indexing::detail
