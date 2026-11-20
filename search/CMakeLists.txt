cmake_minimum_required(VERSION 3.16)
project(boolean_search_labs LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep existing executable/library paths while each feature owns its build definition.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(common modules/common)
add_subdirectory(index modules/index)
add_subdirectory(morphology modules/morphology)
add_subdirectory(boolean_search modules/boolean_search)
add_subdirectory(analysis modules/analysis)

set(SEARCH_BUILD_WEB AUTO CACHE STRING "Build the userver search service (AUTO, ON, or OFF)")
set_property(CACHE SEARCH_BUILD_WEB PROPERTY STRINGS AUTO ON OFF)

if(SEARCH_BUILD_WEB STREQUAL "AUTO")
  find_package(userver 3.2 CONFIG QUIET COMPONENTS core)
  if(NOT TARGET userver::core)
    message(STATUS "userver 3.2 was not found. Building the core only. The Docker build includes the web service.")
  endif()
elseif(SEARCH_BUILD_WEB STREQUAL "ON")
  find_package(userver 3.2 CONFIG REQUIRED COMPONENTS core)
elseif(NOT SEARCH_BUILD_WEB STREQUAL "OFF")
  message(FATAL_ERROR "SEARCH_BUILD_WEB must be AUTO, ON, or OFF")
endif()

if(NOT SEARCH_BUILD_WEB STREQUAL "OFF" AND TARGET userver::core)
  add_subdirectory(service modules/service)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
