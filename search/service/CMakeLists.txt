add_library(search_index_store STATIC src/index_store.cpp)
target_include_directories(search_index_store PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(search_index_store PUBLIC boolean_search_core PRIVATE userver::universal)
target_compile_features(search_index_store PUBLIC cxx_std_20)
target_compile_options(search_index_store PRIVATE -Wall -Wextra -Wpedantic)

add_executable(search_service
  src/main.cpp
  src/search_index.cpp
  src/handlers.cpp
)
target_link_libraries(search_service PRIVATE search_index_store userver::core)
target_compile_options(search_service PRIVATE -Wall -Wextra -Wpedantic)
