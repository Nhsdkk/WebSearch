#include "index_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <istream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "checked_arithmetic.hpp"
#include "index.hpp"
#include "index_format.hpp"
#include "tokenizer.hpp"

namespace indexing {
using detail::CheckedArithmetic;
using detail::IndexFormat;

struct IndexLoader::Directory {
    IndexFormat::Section documents;
    IndexFormat::Section terms;
    IndexFormat::Section postings;
    IndexFormat::Section statistics;
    std::optional<IndexFormat::Section> indexing;

    void Validate(const Header& header) const {
        const bool valid_documents = documents.count == header.document_count &&
                                     documents.count <= documents.size / IndexFormat::DocumentRecord::kPrefixSize;
        const bool valid_terms =
            terms.count == header.term_count && terms.count <= terms.size / IndexFormat::TermRecord::kPrefixSize;
        const auto posting_size = IndexFormat::PostingRecord::kSize;
        const bool valid_postings = postings.size % posting_size == 0 && postings.count == postings.size / posting_size;
        const bool valid_statistics = statistics.size == IndexFormat::StatisticsRecord::kSize && statistics.count == 1;
        if (!valid_documents || !valid_terms || !valid_postings || !valid_statistics) {
            throw std::runtime_error("Inconsistent index section counts");
        }
        if (indexing && (indexing->size != IndexFormat::IndexingRecord::kSize || indexing->count != 1)) {
            throw std::runtime_error("Invalid indexing statistics section");
        }
    }
};

Index IndexLoader::Load(const std::string& path) const {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Cannot open index: " + path);
    }

    const auto length = input.tellg();
    if (length < static_cast<std::streamoff>(Header::kSize) ||
        static_cast<std::uint64_t>(length) > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Invalid index file size");
    }

    input.seekg(0);
    Header header;
    input >> header;
    header.Validate(static_cast<std::uint64_t>(length));
    const auto directory = ReadDirectory(input, header);

    auto documents = ReadDocuments(input, directory.documents);
    std::uint64_t term_characters = 0;
    auto terms = ReadTerms(input, directory, term_characters);
    const auto statistics = ReadStatistics(input, directory, term_characters);

    return Index(std::move(documents), std::move(terms), statistics);
}

IndexLoader::Directory IndexLoader::ReadDirectory(std::istream& input, const Header& header) {
    input.seekg(static_cast<std::streamoff>(header.directory_offset));
    std::vector<IndexFormat::Section> sections;

    for (std::uint64_t i = 0; i < header.section_count; ++i) {
        Section section;
        input >> section;
        section.Validate(header);

        for (const auto& previous : sections) {
            section.ValidateAgainst(previous);
        }
        sections.push_back(section);
    }

    Directory directory{IndexFormat::FindSection(sections, 1), IndexFormat::FindSection(sections, 2),
                        IndexFormat::FindSection(sections, 3), IndexFormat::FindSection(sections, 4), std::nullopt};
    for (const auto& section : sections) {
        if (section.type == 5) {
            directory.indexing = section;
        }
    }
    directory.Validate(header);
    return directory;
}

std::istringstream IndexLoader::ReadSection(std::istream& input, const Section& section) {
    std::string bytes(static_cast<std::size_t>(section.size), '\0');
    input.seekg(static_cast<std::streamoff>(section.offset));
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Could not read complete index section");
    }
    return std::istringstream(std::move(bytes), std::ios::binary | std::ios::in);
}

std::vector<Document> IndexLoader::ReadDocuments(std::istream& input, const Section& section) const {
    auto records = ReadSection(input, section);
    std::vector<Document> documents;
    documents.reserve(static_cast<std::size_t>(section.count));

    for (std::uint64_t i = 0; i < section.count; ++i) {
        IndexFormat::DocumentRecord record;
        records >> record;
        record.Validate(tokenizer_);
        documents.push_back({.title = std::move(record.title), .url = std::move(record.url)});
    }
    if (records.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected bytes in index section");
    }
    return documents;
}

std::vector<Term> IndexLoader::ReadTerms(std::istream& input, const Directory& directory,
                                         std::uint64_t& term_characters) const {
    const auto& section = directory.terms;
    auto records = ReadSection(input, section);
    input.seekg(static_cast<std::streamoff>(directory.postings.offset));

    std::vector<Term> terms;
    terms.reserve(static_cast<std::size_t>(section.count));
    std::uint64_t expected_first = 0;
    term_characters = 0;

    for (std::uint64_t i = 0; i < section.count; ++i) {
        IndexFormat::TermRecord record;
        records >> record;
        const auto previous_term = terms.empty() ? std::string_view{} : std::string_view(terms.back().text);
        record.Validate(tokenizer_, previous_term, expected_first, directory.postings.count, directory.documents.count);

        term_characters = CheckedArithmetic::Add(term_characters, tokenizer_.Utf8Length(record.text));
        expected_first += record.posting_count;
        auto postings = ReadPostings(input, directory.documents.count, record.posting_count);
        terms.push_back({std::move(record.text), std::move(postings)});
    }
    if (records.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected bytes in index section");
    }
    if (expected_first != directory.postings.count) {
        throw std::runtime_error("Unreferenced postings in index");
    }
    return terms;
}

DocList IndexLoader::ReadPostings(std::istream& input, std::uint64_t document_count, std::uint64_t count) {
    DocList postings;
    postings.reserve(static_cast<std::size_t>(count));
    std::optional<DocId> previous_id;

    for (std::uint64_t i = 0; i < count; ++i) {
        IndexFormat::PostingRecord record;
        input >> record;
        record.Validate(document_count, previous_id);
        postings.push_back(record.document_id);
        previous_id = record.document_id;
    }
    return postings;
}

Statistics IndexLoader::ReadStatistics(std::istream& input, const Directory& directory, std::uint64_t term_characters) {
    const auto& section = directory.statistics;
    input.seekg(static_cast<std::streamoff>(section.offset));
    IndexFormat::StatisticsRecord statistics;
    input >> statistics;
    statistics.Validate(term_characters, directory.postings.count);
    Statistics result{.text_bytes = statistics.text_bytes,
                      .token_count = statistics.token_count,
                      .token_characters = statistics.token_characters,
                      .term_characters = statistics.term_characters,
                      .indexing = std::nullopt};
    if (directory.indexing) {
        input.seekg(static_cast<std::streamoff>(directory.indexing->offset));
        IndexFormat::IndexingRecord record;
        input >> record;
        record.Validate();
        result.indexing = IndexingTimings{record.build_nanoseconds, record.write_nanoseconds};
    }
    return result;
}
}  // namespace indexing
