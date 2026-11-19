add_library(boolean_search_core STATIC
  src/query.cpp
  src/query_parser.cpp
)
set_target_properties(boolean_search_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(boolean_search_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(boolean_search_core PUBLIC cxx_std_17)
target_compile_options(boolean_search_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(boolean_search_core PUBLIC search_index)

add_library(boolean_search_cli STATIC src/cli.cpp)
target_include_directories(boolean_search_cli PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(boolean_search_cli PUBLIC boolean_search_core)
target_compile_options(boolean_search_cli PRIVATE -Wall -Wextra -Wpedantic)

add_executable(boolean_search src/cli_main.cpp)
target_link_libraries(boolean_search PRIVATE boolean_search_cli)
