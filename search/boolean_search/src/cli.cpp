#include "boolean_search/cli.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "boolean_search/query.hpp"
#include "index.hpp"
#include "index_loader.hpp"
#include "tokenizer.hpp"

namespace boolsearch {
namespace {

class JsonWriter {
   public:
    explicit JsonWriter(std::ostream& output) : output_(output) {}

    void String(std::string_view text) {
        constexpr char kHex[] = "0123456789abcdef";
        output_ << '"';
        for (const unsigned char character : text) {
            if (character == '"' || character == '\\') {
                output_ << '\\' << static_cast<char>(character);
            } else if (character < 0x20) {
                output_ << "\\u00" << kHex[character >> 4] << kHex[character & 0x0f];
            } else {
                output_ << static_cast<char>(character);
            }
        }
        output_ << '"';
    }

    void Result(const indexing::Index& index, const std::string& query, const indexing::DocList& matches,
                double elapsed_ms) {
        output_ << "{\"query\":";
        String(query);
        output_ << ",\"total\":" << matches.size() << ",\"elapsed_ms\":" << std::fixed << std::setprecision(6)
                << elapsed_ms << ",\"results\":[";
        for (std::size_t i = 0; i < matches.size(); ++i) {
            if (i != 0) {
                output_ << ',';
            }
            const auto id = matches[i];
            const auto& document = index.documents()[id];
            output_ << "{\"id\":" << id << ",\"title\":";
            String(document.title);
            output_ << ",\"url\":";
            String(document.url);
            output_ << '}';
        }
        output_ << "]}\n";
    }

    void Error(const std::string& query, const std::string& message) {
        output_ << "{\"query\":";
        try {
            indexing::Tokenizer{}.Utf8Length(query);
            String(query);
        } catch (const std::runtime_error&) {
            output_ << "null";
        }
        output_ << ",\"error\":";
        String(message);
        output_ << "}\n";
    }

   private:
    std::ostream& output_;
};

}  // namespace

int SearchCli::Run(const std::string& index_path, std::istream& input, std::ostream& output,
                   std::ostream& errors) const {
    try {
        const auto index = indexing::IndexLoader{}.Load(index_path);
        const BooleanSearch search(index);
        bool query_errors = false;
        std::string query;
        while (std::getline(input, query)) {
            if (!query.empty() && query.back() == '\r') {
                query.pop_back();
            }
            std::ostringstream record;
            record.imbue(std::locale::classic());
            JsonWriter writer(record);
            try {
                const auto start = std::chrono::steady_clock::now();
                const auto matches = search.Search(query);
                const auto elapsed =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                writer.Result(index, query, matches, elapsed);
            } catch (const std::runtime_error& error) {
                query_errors = true;
                writer.Error(query, error.what());
            }
            output << record.str();
            if (!output) {
                throw std::runtime_error("Cannot write search results");
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("Cannot read search queries");
        }
        output.flush();
        if (!output) {
            throw std::runtime_error("Cannot finish writing search results");
        }
        return query_errors ? 2 : 0;
    } catch (const std::exception& error) {
        errors << "boolean_search: " << error.what() << '\n';
        return 1;
    }
}

}  // namespace boolsearch
