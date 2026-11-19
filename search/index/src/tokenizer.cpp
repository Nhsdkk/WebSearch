#include "tokenizer.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace indexing {
namespace {
constexpr std::size_t kMaxUtf8CharacterBytes = 4;

utf8proc_int32_t ReadCharacter(std::string_view& remaining) {
    utf8proc_int32_t character = 0;
    const auto available = std::min(remaining.size(), kMaxUtf8CharacterBytes);
    const auto consumed = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(remaining.data()),
                                           static_cast<utf8proc_ssize_t>(available), &character);

    if (consumed < 0) {
        throw std::runtime_error(utf8proc_errmsg(consumed));
    }

    remaining.remove_prefix(static_cast<std::size_t>(consumed));
    return character;
}

bool IsWordCharacter(char32_t character) {
    return (character >= U'A' && character <= U'Z') || (character >= U'a' && character <= U'z') ||
           (character >= U'0' && character <= U'9') || (character >= U'А' && character <= U'я') || character == U'Ё' ||
           character == U'ё';
}
}  // namespace

std::vector<std::string> Tokenizer::Tokenize(const std::string& text) const {
    std::vector<std::string> words;
    std::string word;
    std::string_view remaining = text;

    while (!remaining.empty()) {
        const auto character = ReadCharacter(remaining);
        if (IsWordCharacter(static_cast<char32_t>(character))) {
            utf8proc_uint8_t encoded[kMaxUtf8CharacterBytes];
            const auto length = utf8proc_encode_char(utf8proc_tolower(character), encoded);
            word.append(reinterpret_cast<const char*>(encoded), static_cast<std::size_t>(length));
        } else if (!word.empty()) {
            words.push_back(std::move(word));
            word.clear();
        }
    }

    if (!word.empty()) {
        words.push_back(std::move(word));
    }

    return words;
}

std::uint64_t Tokenizer::Utf8Length(const std::string& text) const {
    std::string_view remaining = text;
    std::uint64_t count = 0;
    while (!remaining.empty()) {
        ReadCharacter(remaining);
        ++count;
    }
    return count;
}
}  // namespace indexing
