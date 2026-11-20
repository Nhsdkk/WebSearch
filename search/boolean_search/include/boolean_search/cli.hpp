#ifndef WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_CLI_HPP_
#define WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_CLI_HPP_

#include <iosfwd>
#include <string>

#include "boolean_search/match_mode.hpp"

namespace boolsearch {

class SearchCli {
   public:
    int Run(const std::string& index_path, std::istream& input, std::ostream& output, std::ostream& errors,
            MatchMode mode = MatchMode::kStem) const;
};

}  // namespace boolsearch

#endif  // WEBSEARCH_SEARCH_BOOLEAN_SEARCH_INCLUDE_BOOLEAN_SEARCH_CLI_HPP_
