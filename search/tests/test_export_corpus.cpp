#include <gtest/gtest.h>

#include <chrono>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "test_files.hpp"
#include "test_process.hpp"

namespace {
using search_test::TestFiles;
using search_test::TestProcess;

class ExportCorpusTest : public ::testing::Test {
   protected:
    std::string Probe(std::initializer_list<std::string> args) {
        std::vector<std::string> command{SEARCH_TEST_PYTHON, "-B", SEARCH_EXPORT_PROBE};
        command.insert(command.end(), args.begin(), args.end());
        TestProcess process(command, files_.Path("probe.log"));
        const auto status = process.WaitFor(std::chrono::seconds(5));
        const auto output = files_.Read("probe.log");
        if (status != 0) {
            throw std::runtime_error("Exporter probe failed\n" + output);
        }
        return output;
    }

    TestFiles files_;
};

TEST_F(ExportCorpusTest, ReadsLiteralEnvironmentValues) {
    const auto env = files_.Write(".env",
                                  "# comment\n"
                                  "DATABASE_USERNAME: user\n"
                                  "DATABASE_PASSWORD: 'a:b=c#d'\n"
                                  "export MONGO_DB_USERNAME=other\n"
                                  "MONGO_DB_PASSWORD=secret:value=literal\n");
    EXPECT_EQ(Probe({"read_environment", env.string(), "DATABASE_USERNAME", "DATABASE_PASSWORD", "MONGO_DB_USERNAME",
                     "MONGO_DB_PASSWORD"}),
              "user\na:b=c#d\nother\nsecret:value=literal");
}

TEST_F(ExportCorpusTest, EscapesFieldBoundaries) {
    EXPECT_EQ(Probe({"escape_field", "Ёж\\n\tA\nB\r"}), "Ёж\\\\n\\tA\\nB\\r");
}

TEST_F(ExportCorpusTest, ValidatesUuids) {
    EXPECT_EQ(Probe({"valid_uuid", "550E8400-E29B-41D4-A716-446655440000"}), "550e8400-e29b-41d4-a716-446655440000");
    EXPECT_EQ(Probe({"valid_uuid", "invalid"}), "");
}
}  // namespace
