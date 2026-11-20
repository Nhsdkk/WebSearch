#ifndef WEBSEARCH_SEARCH_MORPHOLOGY_SRC_RUSSIAN_SUFFIXES_HPP_
#define WEBSEARCH_SEARCH_MORPHOLOGY_SRC_RUSSIAN_SUFFIXES_HPP_

#include <initializer_list>
#include <string_view>

namespace morphology::detail {

// Suffix families from the Russian Snowball rules. Conditional families follow а/я in RV.
class RussianSuffixes {
   public:
    using SuffixList = std::initializer_list<std::u32string_view>;

    static constexpr SuffixList kPerfectiveGerund = {U"ившись", U"ывшись", U"ивши", U"ывши", U"ив", U"ыв"};
    static constexpr SuffixList kConditionalPerfectiveGerund = {U"вшись", U"вши", U"в"};

    static constexpr SuffixList kAdjective = {U"ее", U"ие", U"ые", U"ое", U"ими", U"ыми", U"ей",  U"ий",  U"ый",
                                              U"ой", U"ем", U"им", U"ым", U"ом",  U"его", U"ого", U"ему", U"ому",
                                              U"их", U"ых", U"ую", U"юю", U"ая",  U"яя",  U"ою",  U"ею"};
    static constexpr SuffixList kParticiple = {U"ивш", U"ывш", U"ующ"};
    static constexpr SuffixList kConditionalParticiple = {U"ем", U"нн", U"вш", U"ющ", U"щ"};

    static constexpr SuffixList kVerb = {U"ила", U"ыла", U"ена", U"ейте", U"уйте", U"ите", U"или", U"ыли", U"ей", U"уй",
                                         U"ил",  U"ыл",  U"им",  U"ым",   U"ен",   U"ило", U"ыло", U"ено", U"ят", U"ует",
                                         U"уют", U"ит",  U"ыт",  U"ены",  U"ить",  U"ыть", U"ишь", U"ую",  U"ю"};
    static constexpr SuffixList kConditionalVerb = {U"ла", U"на", U"ете", U"йте", U"ли", U"й",  U"л",   U"ем", U"н",
                                                    U"ло", U"но", U"ет",  U"ют",  U"ны", U"ть", U"ешь", U"нно"};

    static constexpr SuffixList kNoun = {U"а",  U"ев",  U"ов", U"ие",  U"ье", U"е",  U"иями", U"ями", U"ами",
                                         U"еи", U"ии",  U"и",  U"ией", U"ей", U"ой", U"ий",   U"й",   U"иям",
                                         U"ям", U"ием", U"ем", U"ам",  U"ом", U"о",  U"у",    U"ах",  U"иях",
                                         U"ях", U"ы",   U"ь",  U"ию",  U"ью", U"ю",  U"ия",   U"ья",  U"я"};
};

}  // namespace morphology::detail
#endif  // WEBSEARCH_SEARCH_MORPHOLOGY_SRC_RUSSIAN_SUFFIXES_HPP_
