#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZATION_BENCHMARK_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZATION_BENCHMARK_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace indexing {

class TokenizationBenchmark {
   public:
    static constexpr std::uint32_t kDefaultRepetitions = 3;
    static constexpr std::uint32_t kMaxRepetitions = 10;

    struct RunTiming {
        double tokenization_ms = 0;
        double total_ms = 0;
    };

    struct Sample {
        std::uint32_t percentage = 0;
        std::uint64_t documents = 0;
        // Decoded UTF-8 title and body bytes, including word separators.
        std::uint64_t text_bytes = 0;
        std::uint64_t tokens = 0;
        std::uint64_t token_characters = 0;
        std::uint64_t token_bytes = 0;
        std::vector<RunTiming> runs;

        double MeanTokenizationMs() const;
        double MeanTotalMs() const;
        // Rates use Tokenize() time; runs exclude the warm-up.
        double MillisecondsPerKib() const;
        double KibPerSecond() const;
        double AverageTokenCharacters() const;
        double AverageTokenBytes() const;
    };

    struct Report {
        std::uint64_t total_documents = 0;
        std::uint32_t repetitions = kDefaultRepetitions;
        std::vector<Sample> samples;
    };

    // Counts documents, then warms up and measures each rounded 1%, 10%, 100% prefix.
    Report Run(const std::string& corpus_path, std::uint32_t repetitions = kDefaultRepetitions) const;

   private:
    static std::uint64_t CountDocuments(const std::string& path);
    static Sample Measure(const std::string& path, std::uint64_t documents);
    static void ValidateCounts(const Sample& expected, const Sample& actual);
};

}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZATION_BENCHMARK_HPP_
