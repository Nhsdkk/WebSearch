#ifndef SEARCH_TESTS_TEST_FILES_HPP_
#define SEARCH_TESTS_TEST_FILES_HPP_

#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace search_test {

class TestFiles {
   public:
    TestFiles() {
        auto pattern = (std::filesystem::temp_directory_path() / "bidx-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            throw std::runtime_error("Cannot create a test directory");
        }
        path_ = pattern;
    }

    TestFiles(const TestFiles&) = delete;
    TestFiles& operator=(const TestFiles&) = delete;

    ~TestFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    std::filesystem::path Path(std::string_view filename) const { return path_ / filename; }

    std::filesystem::path Write(std::string_view filename, std::string_view bytes) const {
        auto path = Path(filename);
        std::ofstream output(path, std::ios::binary);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write fixture " + path.string());
        }
        return path;
    }

    std::string Read(std::string_view filename) const {
        const auto path = Path(filename);
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot read fixture " + path.string());
        }
        return {std::istreambuf_iterator<char>(input), {}};
    }

   private:
    std::filesystem::path path_;
};

}  // namespace search_test

#endif  // SEARCH_TESTS_TEST_FILES_HPP_
