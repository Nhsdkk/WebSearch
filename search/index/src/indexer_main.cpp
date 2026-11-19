#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ratio>
#include <stdexcept>
#include <string>

#include "index.hpp"
#include "index_saver.hpp"
#include "indexer.hpp"

namespace {
class IndexerApplication {
   public:
    IndexerApplication() = default;
    IndexerApplication(const IndexerApplication&) = delete;
    IndexerApplication& operator=(const IndexerApplication&) = delete;

    int Run(int argc, char** argv) const;

   private:
    using Clock = std::chrono::steady_clock;

    static void ValidatePaths(const std::string& corpus_path, const std::string& index_path);
    static void PrintStatistics(const indexing::Index& index, const std::string& index_path,
                                const indexing::IndexingTimings& timings);

    indexing::Indexer indexer_;
    indexing::IndexSaver saver_;
};

int IndexerApplication::Run(int argc, char** argv) const {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "Usage: boolean_index CORPUS.tsv INDEX.bin\n"
                     "Input: UTF-8 escaped TSV with title, URL, text; no "
                     "header.\n"
                     "Escapes: \\t, \\n, \\r, \\\\. Output: custom binary "
                     "index.\n";
        return 0;
    }
    if (argc != 3) {
        std::cerr << "Usage: boolean_index CORPUS.tsv INDEX.bin\n";
        return 1;
    }
    try {
        ValidatePaths(argv[1], argv[2]);

        const auto start = Clock::now();
        const auto index = indexer_.Build(argv[1]);
        const auto built = Clock::now();
        const auto build_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(built - start).count();
        const auto timings = saver_.Save(index, argv[2], build_nanoseconds);
        PrintStatistics(index, argv[2], timings.value());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "boolean_index: " << e.what() << '\n';
        return 1;
    }
}

void IndexerApplication::ValidatePaths(const std::string& corpus_path, const std::string& index_path) {
    if (std::filesystem::absolute(corpus_path).lexically_normal() ==
        std::filesystem::absolute(index_path).lexically_normal()) {
        throw std::runtime_error("Corpus and index paths must differ");
    }
    if (std::filesystem::exists(index_path) && std::filesystem::equivalent(corpus_path, index_path)) {
        throw std::runtime_error("Corpus and index must be different files");
    }
}

void IndexerApplication::PrintStatistics(const indexing::Index& index, const std::string& index_path,
                                         const indexing::IndexingTimings& timings) {
    const double build_ms = static_cast<double>(timings.build_nanoseconds) / 1e6;
    const double write_ms = static_cast<double>(timings.write_nanoseconds) / 1e6;
    const double terms = static_cast<double>(index.terms().size());
    const double tokens = static_cast<double>(index.statistics().token_count);
    std::cout << std::fixed << std::setprecision(6) << "{\n  \"documents\": " << index.documents().size()
              << ",\n  \"terms\": " << index.terms().size() << ",\n  \"tokens\": " << index.statistics().token_count
              << ",\n  \"text_bytes\": " << index.statistics().text_bytes
              << ",\n  \"index_bytes\": " << std::filesystem::file_size(index_path)
              << ",\n  \"average_term_characters\": " << (terms ? index.statistics().term_characters / terms : 0.0)
              << ",\n  \"average_token_characters\": " << (tokens ? index.statistics().token_characters / tokens : 0.0)
              << ",\n  \"build_ms\": " << build_ms << ",\n  \"write_ms\": " << write_ms
              << ",\n  \"total_seconds\": " << timings.TotalSeconds()
              << ",\n  \"ms_per_document\": " << timings.MillisecondsPerDocument(index.documents().size())
              << ",\n  \"ms_per_kib\": " << timings.MillisecondsPerKib(index.statistics().text_bytes)
              << ",\n  \"documents_per_second\": " << timings.DocumentsPerSecond(index.documents().size())
              << ",\n  \"kib_per_second\": " << timings.KibPerSecond(index.statistics().text_bytes) << "\n}\n";
    if (!std::cout) {
        throw std::runtime_error("Failed writing indexing statistics");
    }
}
}  // namespace

int main(int argc, char** argv) { return IndexerApplication{}.Run(argc, argv); }
