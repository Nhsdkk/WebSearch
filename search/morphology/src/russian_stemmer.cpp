#include "russian_stemmer.hpp"

#include <utf8proc.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

#include "russian_suffixes.hpp"

namespace morphology {
using detail::RussianSuffixes;

std::string RussianStemmer::Stem(const std::string& token) const {
    auto word = Decode(token);
    if (!NormalizeRussian(word)) {
        return token;
    }

    const auto regions = MarkRegions(word);
    RemoveInflections(word, regions.rv);
    Remove(word, regions.rv, {U"и"});
    Remove(word, regions.r2, {U"ость", U"ост"});
    TidyEnding(word, regions.rv);

    return Encode(word);
}

bool RussianStemmer::NormalizeRussian(std::u32string& word) {
    for (const auto character : word) {
        if (!(character >= U'а' && character <= U'я') && character != U'ё') {
            return false;
        }
    }

    for (auto& character : word) {
        if (character == U'ё') {
            character = U'е';
        }
    }
    return true;
}

RussianStemmer::Regions RussianStemmer::MarkRegions(const std::u32string& word) {
    auto rv = word.size();
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (IsVowel(word[i])) {
            rv = i + 1;
            break;
        }
    }

    const auto r1 = Region(word, 0);
    return {rv, Region(word, r1)};
}

void RussianStemmer::RemoveInflections(std::u32string& word, std::size_t rv) {
    if (Remove(word, rv, RussianSuffixes::kPerfectiveGerund) ||
        Remove(word, rv, RussianSuffixes::kConditionalPerfectiveGerund, true)) {
        return;
    }

    Remove(word, rv, {U"ся", U"сь"});
    if (RemoveAdjectival(word, rv)) {
        return;
    }

    if (Remove(word, rv, RussianSuffixes::kVerb) || Remove(word, rv, RussianSuffixes::kConditionalVerb, true)) {
        return;
    }
    Remove(word, rv, RussianSuffixes::kNoun);
}

bool RussianStemmer::RemoveAdjectival(std::u32string& word, std::size_t rv) {
    if (!Remove(word, rv, RussianSuffixes::kAdjective)) {
        return false;
    }

    if (!Remove(word, rv, RussianSuffixes::kParticiple)) {
        Remove(word, rv, RussianSuffixes::kConditionalParticiple, true);
    }
    return true;
}

void RussianStemmer::TidyEnding(std::u32string& word, std::size_t rv) {
    if (!Remove(word, rv, {U"нн"})) {
        if (Remove(word, rv, {U"ейше", U"ейш"})) {
            if (word.size() >= rv + 2 && word.compare(word.size() - 2, 2, U"нн") == 0) {
                word.pop_back();
            }
        } else {
            Remove(word, rv, {U"ь"});
        }
    } else {
        word.push_back(U'н');
    }
}

bool RussianStemmer::IsVowel(char32_t character) {
    return std::u32string_view(U"аеиоуыэюя").find(character) != std::u32string_view::npos;
}

std::size_t RussianStemmer::Region(const std::u32string& word, std::size_t start) {
    for (auto i = start + 1; i < word.size(); ++i) {
        if (IsVowel(word[i - 1]) && !IsVowel(word[i])) {
            return i + 1;
        }
    }
    return word.size();
}

bool RussianStemmer::Remove(std::u32string& word, std::size_t region,
                            std::initializer_list<std::u32string_view> suffixes, bool after_a_or_ya) {
    std::u32string_view longest;
    for (const auto suffix : suffixes) {
        if (suffix.size() > word.size() || suffix.size() <= longest.size()) {
            continue;
        }

        const auto begin = word.size() - suffix.size();
        if (begin < region || word.compare(begin, suffix.size(), suffix) != 0) {
            continue;
        }
        if (after_a_or_ya && (begin <= region || (word[begin - 1] != U'а' && word[begin - 1] != U'я'))) {
            continue;
        }
        longest = suffix;
    }

    if (longest.empty()) {
        return false;
    }
    word.resize(word.size() - longest.size());
    return true;
}

std::u32string RussianStemmer::Decode(const std::string& token) {
    std::u32string word;
    for (std::size_t position = 0; position < token.size();) {
        utf8proc_int32_t character = 0;
        const auto remaining = token.size() - position;
        const auto bytes = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(token.data() + position),
                                            static_cast<utf8proc_ssize_t>(remaining < 4 ? remaining : 4), &character);
        if (bytes < 0) {
            throw std::runtime_error("Invalid UTF-8 in stemmer input");
        }

        word.push_back(static_cast<char32_t>(character));
        position += static_cast<std::size_t>(bytes);
    }
    return word;
}

std::string RussianStemmer::Encode(const std::u32string& word) {
    std::string token;
    for (const auto character : word) {
        utf8proc_uint8_t bytes[4];
        const auto count = utf8proc_encode_char(static_cast<utf8proc_int32_t>(character), bytes);
        token.append(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(count));
    }
    return token;
}

}  // namespace morphology
