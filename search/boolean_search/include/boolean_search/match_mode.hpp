#ifndef WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_MATCH_MODE_HPP_
#define WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_MATCH_MODE_HPP_

#include <stdexcept>
#include <string_view>

namespace boolsearch {
enum class MatchMode { kStem, kExact };

class MatchModes {
   public:
    static MatchMode Parse(std::string_view text) {
        if (text == "stem") {
            return MatchMode::kStem;
        }
        if (text == "exact") {
            return MatchMode::kExact;
        }
        throw std::invalid_argument("Mode must be stem or exact");
    }
    static std::string_view Name(MatchMode mode) { return mode == MatchMode::kStem ? "stem" : "exact"; }
};
}  // namespace boolsearch
#endif  // WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_MATCH_MODE_HPP_
