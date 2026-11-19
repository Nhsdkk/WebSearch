#include "corpus_reader.hpp"

#include <exception>
#include <ios>
#include <stdexcept>
#include <string>

#include "tokenizer.hpp"

namespace indexing {

CorpusReader::CorpusReader(const std::string& path) : input_(path, std::ios::binary), path_(path) {
    if (!input_) {
        throw std::runtime_error("Cannot open corpus: " + path);
    }
}

bool CorpusReader::Next(CorpusDocument& document) {
    std::string line;
    while (std::getline(input_, line)) {
        ++line_number_;
        if (line_number_ == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) {
            line.erase(0, 3);
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        try {
            document = ParseDocument(line);
            Tokenizer{}.Utf8Length(document.url);
            return true;
        } catch (const std::exception& error) {
            throw std::runtime_error("Corpus line " + std::to_string(line_number_) + ": " + error.what());
        }
    }
    if (!input_.eof()) {
        throw std::runtime_error("Failed to read corpus: " + path_);
    }
    return false;
}

CorpusDocument CorpusReader::ParseDocument(const std::string& line) {
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos || line.find('\t', second + 1) != std::string::npos) {
        throw std::runtime_error("Expected exactly three TSV fields: title, URL, text");
    }
    CorpusDocument document{Unescape(line.substr(0, first)), Unescape(line.substr(first + 1, second - first - 1)),
                            Unescape(line.substr(second + 1))};
    if (document.title.empty() || document.url.empty()) {
        throw std::runtime_error("Title and URL must not be empty");
    }
    return document;
}

std::string CorpusReader::Unescape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\') {
            out.push_back(text[i]);
            continue;
        }
        if (++i == text.size()) {
            throw std::runtime_error("Trailing backslash in TSV field");
        }
        switch (text[i]) {
            case '\\':
                out.push_back('\\');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            default:
                throw std::runtime_error("Unknown TSV escape (use \\\\, \\t, \\n, or \\r)");
        }
    }
    return out;
}

}  // namespace indexing
