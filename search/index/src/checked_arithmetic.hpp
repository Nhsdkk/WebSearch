#ifndef WEBSEARCH_SEARCH_INDEX_SRC_CHECKED_ARITHMETIC_HPP_
#define WEBSEARCH_SEARCH_INDEX_SRC_CHECKED_ARITHMETIC_HPP_

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace indexing::detail {
// Checks byte-count arithmetic before writing or validating file offsets.
class CheckedArithmetic {
   public:
    static constexpr std::uint32_t kMaxUint32 = std::numeric_limits<std::uint32_t>::max();

    static std::uint64_t Add(std::uint64_t a, std::uint64_t b) {
        if (b > std::numeric_limits<std::uint64_t>::max() - a) {
            throw std::runtime_error("Index size overflow");
        }
        return a + b;
    }
};
}  // namespace indexing::detail

#endif  // WEBSEARCH_SEARCH_INDEX_SRC_CHECKED_ARITHMETIC_HPP_
