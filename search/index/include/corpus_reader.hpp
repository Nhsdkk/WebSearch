#ifndef WEBSEARCH_SEARCH_INDEX_INCLUDE_CORPUS_READER_HPP_
#define WEBSEARCH_SEARCH_INDEX_INCLUDE_CORPUS_READER_HPP_

#include <cstddef>
#include <fstream>
#include <string>

namespace indexing {

struct CorpusDocument {
    std::string title;
    std::string url;
    std::string text;
};

class CorpusReader {
   public:
    explicit CorpusReader(const std::string& path);

    // Skips blank physical lines. Returns false at EOF; errors include the line number.
    bool Next(CorpusDocument& document);
    std::size_t line_number() const { return line_number_; }

   private:
    static CorpusDocument ParseDocument(const std::string& line);
    static std::string Unescape(const std::string& text);

    std::ifstream input_;
    std::string path_;
    std::size_t line_number_ = 0;
};

}  // namespace indexing

#endif  // WEBSEARCH_SEARCH_INDEX_INCLUDE_CORPUS_READER_HPP_
