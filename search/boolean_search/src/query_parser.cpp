#include "query_parser.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "boolean_search/query.hpp"
#include "index.hpp"
#include "tokenizer.hpp"

namespace boolsearch {
using indexing::DocList;

DocList BooleanSearch::Parser::Run() {
    if (current() == Kind::kEnd) {
        return {};
    }
    auto result = ParseExpression(0);
    if (current() != Kind::kEnd) {
        Fail("Unexpected token (unmatched closing parenthesis?)");
    }
    return result;
}

std::vector<BooleanSearch::Parser::Token> BooleanSearch::Parser::Lex(const std::string& query) const {
    if (query.size() > 65536) {
        throw std::runtime_error("Query exceeds 65536 bytes");
    }
    std::vector<Token> tokens;
    for (std::size_t i = 0; i < query.size();) {
        const std::size_t start = i;
        const char c = query[i];
        if (c == '&' || c == '|') {
            if (i + 1 >= query.size() || query[i + 1] != c) {
                throw std::runtime_error("Expected doubled operator at byte " + std::to_string(i + 1));
            }
            tokens.push_back({c == '&' ? Kind::kAnd : Kind::kOr, {}, i});
            i += 2;
        } else if (c == '!' || c == '(' || c == ')') {
            tokens.push_back({c == '!' ? Kind::kNot : c == '(' ? Kind::kOpen : Kind::kClose, {}, i});
            ++i;
        } else {
            while (i < query.size() && !IsSpecial(query[i])) {
                ++i;
            }
            for (auto& word : tokenizer_.Tokenize(query.substr(start, i - start))) {
                tokens.push_back({Kind::kWord, std::move(word), start});
            }
        }
        if (tokens.size() > 4096) {
            throw std::runtime_error("Query exceeds 4096 tokens");
        }
    }
    tokens.push_back({Kind::kEnd, {}, query.size()});
    return tokens;
}

DocList BooleanSearch::Parser::ParseExpression(unsigned depth) {
    auto result = ParseConjunction(depth);
    while (current() == Kind::kOr) {
        ++cursor_;
        result = Unite(result, ParseConjunction(depth));
    }

    return result;
}

DocList BooleanSearch::Parser::ParseConjunction(unsigned depth) {
    auto result = ParseUnary(depth);
    while (current() == Kind::kAnd || StartsOperand(current())) {
        if (current() == Kind::kAnd) {
            ++cursor_;
        }
        result = Intersect(result, ParseUnary(depth));
    }

    return result;
}

DocList BooleanSearch::Parser::ParseUnary(unsigned depth) {
    bool negative = false;
    while (current() == Kind::kNot) {
        negative = !negative;
        ++cursor_;
    }

    DocList result;
    if (current() == Kind::kWord) {
        result = index_.Lookup(tokens_[cursor_++].word);
    } else if (current() == Kind::kOpen) {
        if (depth >= 128) {
            Fail("Parentheses nesting exceeds 128 levels");
        }
        ++cursor_;
        result = ParseExpression(depth + 1);
        if (current() != Kind::kClose) {
            Fail("Expected closing parenthesis");
        }
        ++cursor_;
    } else {
        Fail("Expected a term, !, or opening parenthesis");
    }
    return negative ? Complement(result, index_.documents().size()) : result;
}
}  // namespace boolsearch
