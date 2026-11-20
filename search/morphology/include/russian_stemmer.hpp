#ifndef WEBSEARCH_SEARCH_MORPHOLOGY_INCLUDE_RUSSIAN_STEMMER_HPP_
#define WEBSEARCH_SEARCH_MORPHOLOGY_INCLUDE_RUSSIAN_STEMMER_HPP_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace morphology {

// Independent implementation of the Snowball Russian rules, not a dictionary lemmatizer.
class RussianStemmer {
   public:
    // Input is a lowercase tokenizer token. Non-Russian tokens are returned unchanged.
    // Rejects malformed UTF-8. Russian words fold ё to е before suffix removal.
    std::string Stem(const std::string& token) const;

   private:
    struct Regions {
        std::size_t rv;
        std::size_t r2;
    };

    static bool NormalizeRussian(std::u32string& word);
    static Regions MarkRegions(const std::u32string& word);
    static void RemoveInflections(std::u32string& word, std::size_t rv);
    static bool RemoveAdjectival(std::u32string& word, std::size_t rv);
    static void TidyEnding(std::u32string& word, std::size_t rv);

    static bool IsVowel(char32_t character);
    static std::size_t Region(const std::u32string& word, std::size_t start);
    static bool Remove(std::u32string& word, std::size_t region, std::initializer_list<std::u32string_view> suffixes,
                       bool after_a_or_ya = false);

    static std::u32string Decode(const std::string& token);
    static std::string Encode(const std::u32string& word);
};

}  // namespace morphology
#endif  // WEBSEARCH_SEARCH_MORPHOLOGY_INCLUDE_RUSSIAN_STEMMER_HPP_
