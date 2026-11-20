#include <gtest/gtest.h>

#include <chrono>

#include "test_files.hpp"
#include "test_process.hpp"

namespace {
TEST(FrontendTest, PreservesModesChartsAndIndexReplacementState) {
    search_test::TestFiles files;
    search_test::TestProcess process({SEARCH_TEST_NODE, SEARCH_FRONTEND_PROBE, SEARCH_FRONTEND_HTML},
                                     files.Path("ui.log"));
    EXPECT_EQ(process.WaitFor(std::chrono::seconds(15)), 0) << files.Read("ui.log");
}
}  // namespace
