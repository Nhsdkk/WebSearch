#include <exception>
#include <iostream>
#include <string_view>

#include "boolean_search/cli.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")) {
        std::cout << "Usage: boolean_search INDEX.bin [--mode stem|exact] < queries.txt > results.jsonl\n";
        return 0;
    }
    if (argc != 2 && !(argc == 4 && std::string_view(argv[2]) == "--mode")) {
        std::cerr << "Usage: boolean_search INDEX.bin [--mode stem|exact] < queries.txt > results.jsonl\n";
        return 1;
    }
    try {
        const auto mode = argc == 4 ? boolsearch::MatchModes::Parse(argv[3]) : boolsearch::MatchMode::kStem;
        return boolsearch::SearchCli{}.Run(argv[1], std::cin, std::cout, std::cerr, mode);
    } catch (const std::exception& error) {
        std::cerr << "boolean_search: " << error.what() << '\n';
        return 1;
    }
}
