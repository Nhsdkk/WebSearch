#ifndef WEBSEARCH_SEARCH_BOOLEAN_SEARCH_SRC_QUERY_PARSER_HPP_
#define WEBSEARCH_SEARCH_BOOLEAN_SEARCH_SRC_QUERY_PARSER_HPP_

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "boolean_search/query.hpp"
#include "index.hpp"
#include "tokenizer.hpp"

namespace boolsearch {
// Parses one query with precedence NOT, AND, OR over a borrowed immutable
// index.
class BooleanSearch::Parser {
   public:
    Parser(const indexing::Index& index, const StemIndex& stems, const std::string& query, MatchMode mode)
        : index_(index), stems_(stems), mode_(mode), tokens_(Lex(query)) {}

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    indexing::DocList Run();

   private:
    enum class Kind { kWord, kAnd, kOr, kNot, kOpen, kClose, kEnd };

    struct Token {
        Kind kind;
        std::string word;
        std::size_t position;
    };

    static bool IsSpecial(char c) { return c == '&' || c == '|' || c == '!' || c == '(' || c == ')'; }

    std::vector<Token> Lex(const std::string& query) const;

    Kind current() const { return tokens_[cursor_].kind; }

    [[noreturn]] void Fail(const std::string& reason) const {
        throw std::runtime_error(reason + " at byte " + std::to_string(tokens_[cursor_].position + 1));
    }

    static bool StartsOperand(Kind kind) { return kind == Kind::kWord || kind == Kind::kOpen || kind == Kind::kNot; }

    indexing::DocList ParseExpression(unsigned depth);
    indexing::DocList ParseConjunction(unsigned depth);
    indexing::DocList ParseUnary(unsigned depth);

    const indexing::Index& index_;
    const StemIndex& stems_;
    MatchMode mode_;
    indexing::Tokenizer tokenizer_;
    std::vector<Token> tokens_;
    std::size_t cursor_ = 0;
};
}  // namespace boolsearch

#endif  // WEBSEARCH_SEARCH_BOOLEAN_SEARCH_SRC_QUERY_PARSER_HPP_
