#include "index_saver.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "checked_arithmetic.hpp"
#include "index.hpp"
#include "index_format.hpp"

namespace indexing {
using detail::CheckedArithmetic;
using detail::IndexFormat;

struct IndexSaver::Layout {
    IndexFormat::Header header;
    std::vector<IndexFormat::Section> sections;
};

std::optional<IndexingTimings> IndexSaver::Save(const Index& index, const std::string& path,
                                                std::optional<std::uint64_t> build_nanoseconds) const {
    const auto start = std::chrono::steady_clock::now();
    auto timings = index.statistics().indexing;
    if (build_nanoseconds) {
        timings = IndexingTimings{*build_nanoseconds, 0};
    }
    const auto layout = CalculateLayout(index, timings.has_value());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot open index for writing: " + path);
    }

    output << layout.header;
    WriteDirectory(output, layout);

    WriteDocuments(output, index.documents());
    WriteTerms(output, index.terms());
    WritePostings(output, index.terms());
    WriteStatistics(output, index.statistics());

    if (timings) {
        IndexFormat::IndexingRecord record{timings->build_nanoseconds, timings->write_nanoseconds};
        record.Validate();
        output << record;
        if (build_nanoseconds) {
            output.flush();
            const auto elapsed = std::chrono::steady_clock::now() - start;
            record.write_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
            record.Validate();
            timings->write_nanoseconds = record.write_nanoseconds;
            output.seekp(static_cast<std::streamoff>(layout.sections.back().offset));
            output << record;
        }
    }

    output.close();
    if (!output) {
        throw std::runtime_error("Could not finish writing index");
    }
    return timings;
}

IndexSaver::Layout IndexSaver::CalculateLayout(const Index& index, bool include_indexing) {
    if (index.documents().size() > CheckedArithmetic::kMaxUint32) {
        throw std::runtime_error("Too many documents for 32-bit IDs");
    }

    std::uint64_t docs_size = 0;
    for (const auto& doc : index.documents()) {
        docs_size = CheckedArithmetic::Add(docs_size, IndexFormat::DocumentRecord::SizeOf(doc.title, doc.url));
    }

    std::uint64_t terms_size = 0;
    std::uint64_t posting_count = 0;
    for (const auto& term : index.terms()) {
        terms_size = CheckedArithmetic::Add(terms_size, IndexFormat::TermRecord::SizeOf(term.text));
        posting_count = CheckedArithmetic::Add(posting_count, term.postings.size());
    }
    if (posting_count > std::numeric_limits<std::uint64_t>::max() / IndexFormat::PostingRecord::kSize) {
        throw std::runtime_error("Posting size overflow");
    }

    IndexFormat::Header header;
    header.document_count = index.documents().size();
    header.term_count = index.terms().size();
    if (include_indexing) {
        header.minor_version = 1;
        ++header.section_count;
    }

    const auto docs_offset = header.directory_offset + header.section_count * IndexFormat::Section::kSize;
    const auto terms_offset = CheckedArithmetic::Add(docs_offset, docs_size);
    const auto postings_offset = CheckedArithmetic::Add(terms_offset, terms_size);
    const auto postings_size = posting_count * IndexFormat::PostingRecord::kSize;
    const auto stats_offset = CheckedArithmetic::Add(postings_offset, postings_size);
    header.file_size = CheckedArithmetic::Add(stats_offset, IndexFormat::StatisticsRecord::kSize);

    Layout layout{header,
                  {{1, 0, docs_offset, docs_size, header.document_count},
                   {2, 0, terms_offset, terms_size, header.term_count},
                   {3, 0, postings_offset, postings_size, posting_count},
                   {4, 0, stats_offset, IndexFormat::StatisticsRecord::kSize, 1}}};
    if (include_indexing) {
        layout.sections.push_back({5, 0, header.file_size, IndexFormat::IndexingRecord::kSize, 1});
        layout.header.file_size = CheckedArithmetic::Add(header.file_size, IndexFormat::IndexingRecord::kSize);
    }
    return layout;
}

void IndexSaver::WriteDirectory(std::ostream& out, const Layout& layout) {
    for (const auto& section : layout.sections) {
        out << section;
    }
}

void IndexSaver::WriteDocuments(std::ostream& out, const std::vector<Document>& documents) {
    for (const auto& doc : documents) {
        const IndexFormat::DocumentRecord record{doc.title, doc.url};
        out << record;
    }
}

void IndexSaver::WriteTerms(std::ostream& out, const std::vector<Term>& terms) {
    std::uint64_t first_posting = 0;
    for (const auto& term : terms) {
        const IndexFormat::TermRecord record{term.text, 0, first_posting, term.postings.size()};
        out << record;
        first_posting += term.postings.size();
    }
}

void IndexSaver::WritePostings(std::ostream& out, const std::vector<Term>& terms) {
    for (const auto& term : terms) {
        for (DocId doc : term.postings) {
            const IndexFormat::PostingRecord record{doc};
            out << record;
        }
    }
}

void IndexSaver::WriteStatistics(std::ostream& out, const Statistics& statistics) {
    const IndexFormat::StatisticsRecord record{statistics.text_bytes, statistics.token_count,
                                               statistics.token_characters, statistics.term_characters};
    out << record;
}
}  // namespace indexing
