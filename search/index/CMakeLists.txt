# Corpus processing and binary index storage, independent of Boolean search.
find_path(UTF8PROC_INCLUDE_DIR NAMES utf8proc.h)
find_library(UTF8PROC_LIBRARY NAMES utf8proc)
if(NOT UTF8PROC_INCLUDE_DIR OR NOT UTF8PROC_LIBRARY)
  message(FATAL_ERROR "utf8proc headers and library are required. Install utf8proc or set CMAKE_PREFIX_PATH to its installation directory.")
endif()

add_library(search_index STATIC
  src/index.cpp
  src/tokenizer.cpp
  src/corpus_reader.cpp
  src/tokenization_benchmark.cpp
  src/indexer.cpp
  src/index_format.cpp
  src/index_saver.cpp
  src/index_loader.cpp
)
set_target_properties(search_index PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(search_index PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(search_index SYSTEM PRIVATE "${UTF8PROC_INCLUDE_DIR}")
target_link_libraries(search_index PRIVATE "${UTF8PROC_LIBRARY}")
target_compile_features(search_index PRIVATE cxx_std_20 PUBLIC cxx_std_17)
target_compile_options(search_index PRIVATE -Wall -Wextra -Wpedantic)

add_executable(boolean_index src/indexer_main.cpp)
target_link_libraries(boolean_index PRIVATE search_index)
