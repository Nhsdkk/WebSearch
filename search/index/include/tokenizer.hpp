#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZER_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZER_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace indexing {
// Tokenizes Latin/Russian text with strict UTF-8 decoding.
class Tokenizer {
   public:
    Tokenizer() = default;
    Tokenizer(const Tokenizer&) = default;
    Tokenizer& operator=(const Tokenizer&) = default;

    // Splits valid UTF-8 into lowercase words; rejects malformed UTF-8.
    std::vector<std::string> Tokenize(const std::string& text) const;

    // Validates UTF-8 and counts Unicode code points, not bytes.
    std::uint64_t Utf8Length(const std::string& text) const;
};
}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_TOKENIZER_HPP_
