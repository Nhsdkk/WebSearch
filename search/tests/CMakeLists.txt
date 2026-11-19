if(NOT TARGET GTest::gtest_main)
  find_package(GTest CONFIG REQUIRED)
endif()
include(GoogleTest)

add_executable(test_search test_search.cpp)
target_link_libraries(test_search PRIVATE boolean_search_core GTest::gtest_main)
gtest_discover_tests(test_search)

add_executable(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE boolean_search_cli GTest::gtest_main)
gtest_discover_tests(test_cli)

add_executable(test_index_format test_index_format.cpp)
target_link_libraries(test_index_format PRIVATE search_index GTest::gtest_main)
gtest_discover_tests(test_index_format)

add_executable(test_tokenization test_corpus_reader.cpp test_tokenization_benchmark.cpp)
target_link_libraries(test_tokenization PRIVATE search_index GTest::gtest_main)
gtest_discover_tests(test_tokenization)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(test_export_corpus test_export_corpus.cpp)
target_link_libraries(test_export_corpus PRIVATE GTest::gtest_main)
target_compile_definitions(test_export_corpus PRIVATE
  SEARCH_TEST_PYTHON="${Python3_EXECUTABLE}"
  SEARCH_EXPORT_PROBE="${CMAKE_CURRENT_SOURCE_DIR}/export_probe.py"
)
gtest_discover_tests(test_export_corpus PROPERTIES TIMEOUT 30)

if(TARGET search_service)
  add_executable(test_index_store test_index_store.cpp)
  target_link_libraries(test_index_store PRIVATE search_index_store GTest::gtest_main)
  gtest_discover_tests(test_index_store)
  if(NOT TARGET CURL::libcurl)
    find_package(CURL REQUIRED)
  endif()
  add_executable(test_web_api test_web_api.cpp)
  target_link_libraries(test_web_api PRIVATE userver::universal CURL::libcurl GTest::gtest_main)
  target_compile_definitions(test_web_api PRIVATE
    SEARCH_INDEXER_EXECUTABLE="$<TARGET_FILE:boolean_index>"
    SEARCH_SERVICE_EXECUTABLE="$<TARGET_FILE:search_service>"
    SEARCH_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_dependencies(test_web_api boolean_index search_service)
  gtest_discover_tests(test_web_api PROPERTIES TIMEOUT 45)
endif()
