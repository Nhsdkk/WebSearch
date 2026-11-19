#include "tokenization_benchmark.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <ratio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "checked_arithmetic.hpp"
#include "corpus_reader.hpp"
#include "tokenizer.hpp"

namespace indexing {
using detail::CheckedArithmetic;
using Clock = std::chrono::steady_clock;

double TokenizationBenchmark::Sample::MeanTokenizationMs() const {
    double total = 0;
    for (const auto& run : runs) {
        total += run.tokenization_ms;
    }
    return runs.empty() ? 0.0 : total / static_cast<double>(runs.size());
}

double TokenizationBenchmark::Sample::MeanTotalMs() const {
    double total = 0;
    for (const auto& run : runs) {
        total += run.total_ms;
    }
    return runs.empty() ? 0.0 : total / static_cast<double>(runs.size());
}

double TokenizationBenchmark::Sample::MillisecondsPerKib() const {
    return text_bytes == 0 ? 0.0 : MeanTokenizationMs() / (static_cast<double>(text_bytes) / 1024.0);
}

double TokenizationBenchmark::Sample::KibPerSecond() const {
    const auto milliseconds = MeanTokenizationMs();
    return milliseconds == 0 ? 0.0 : static_cast<double>(text_bytes) / 1024.0 / (milliseconds / 1000.0);
}

double TokenizationBenchmark::Sample::AverageTokenCharacters() const {
    return tokens == 0 ? 0.0 : static_cast<double>(token_characters) / static_cast<double>(tokens);
}

double TokenizationBenchmark::Sample::AverageTokenBytes() const {
    return tokens == 0 ? 0.0 : static_cast<double>(token_bytes) / static_cast<double>(tokens);
}

std::uint64_t TokenizationBenchmark::CountDocuments(const std::string& path) {
    CorpusReader reader(path);
    CorpusDocument document;
    std::uint64_t count = 0;
    while (reader.Next(document)) {
        count = CheckedArithmetic::Add(count, 1);
    }
    return count;
}

TokenizationBenchmark::Sample TokenizationBenchmark::Measure(const std::string& path, std::uint64_t documents) {
    const auto start = Clock::now();
    Sample sample;
    RunTiming timing;
    {
        CorpusReader reader(path);
        CorpusDocument document;
        const Tokenizer tokenizer;
        while (sample.documents < documents && reader.Next(document)) {
            try {
                for (const auto* field : {&document.title, &document.text}) {
                    const auto tokenization_start = Clock::now();
                    const auto tokens = tokenizer.Tokenize(*field);
                    timing.tokenization_ms +=
                        std::chrono::duration<double, std::milli>(Clock::now() - tokenization_start).count();
                    sample.text_bytes = CheckedArithmetic::Add(sample.text_bytes, field->size());
                    sample.tokens = CheckedArithmetic::Add(sample.tokens, tokens.size());
                    for (const auto& token : tokens) {
                        sample.token_characters =
                            CheckedArithmetic::Add(sample.token_characters, tokenizer.Utf8Length(token));
                        sample.token_bytes = CheckedArithmetic::Add(sample.token_bytes, token.size());
                    }
                }
                ++sample.documents;
            } catch (const std::exception& error) {
                throw std::runtime_error("Corpus line " + std::to_string(reader.line_number()) + ": " + error.what());
            }
        }
        if (sample.documents != documents) {
            throw std::runtime_error("Corpus changed during tokenization benchmark");
        }
    }
    timing.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    sample.runs.push_back(timing);
    return sample;
}

void TokenizationBenchmark::ValidateCounts(const Sample& expected, const Sample& actual) {
    if (actual.documents != expected.documents || actual.text_bytes != expected.text_bytes ||
        actual.tokens != expected.tokens || actual.token_characters != expected.token_characters ||
        actual.token_bytes != expected.token_bytes) {
        throw std::runtime_error("Corpus changed during tokenization benchmark");
    }
}

TokenizationBenchmark::Report TokenizationBenchmark::Run(const std::string& corpus_path,
                                                         std::uint32_t repetitions) const {
    if (repetitions == 0 || repetitions > kMaxRepetitions) {
        throw std::invalid_argument("Tokenization repetitions must be between 1 and 10");
    }
    Report report;
    report.total_documents = CountDocuments(corpus_path);
    report.repetitions = repetitions;
    for (const std::uint32_t percentage : {1, 10, 100}) {
        const auto documents =
            (report.total_documents / 100) * percentage + (report.total_documents % 100 * percentage + 99) / 100;
        auto sample = Measure(corpus_path, documents);
        sample.percentage = percentage;
        sample.runs.clear();
        for (std::uint32_t i = 0; i < repetitions; ++i) {
            const auto measured = Measure(corpus_path, documents);
            ValidateCounts(sample, measured);
            sample.runs.push_back(measured.runs.front());
        }
        report.samples.push_back(std::move(sample));
    }
    return report;
}

}  // namespace indexing
