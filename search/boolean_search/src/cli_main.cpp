#include <iostream>
#include <string_view>

#include "boolean_search/cli.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")) {
        std::cout << "Usage: boolean_search INDEX.bin < queries.txt > results.jsonl\n";
        return 0;
    }
    if (argc != 2) {
        std::cerr << "Usage: boolean_search INDEX.bin < queries.txt > results.jsonl\n";
        return 1;
    }
    return boolsearch::SearchCli{}.Run(argv[1], std::cin, std::cout, std::cerr);
}
