# Unit Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.22)

project(magda_unit_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Include WDL headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL
    ${CMAKE_CURRENT_SOURCE_DIR}/../../WDL/WDL
)

# DSL Tokenizer and Params tests (standalone, no REAPER dependencies)
add_executable(test_dsl_parser
    test_dsl_parser.cpp
)
target_link_libraries(test_dsl_parser GTest::gtest_main)

# JSON parsing tests
add_executable(test_json_parsing
    test_json_parsing.cpp
)
target_link_libraries(test_json_parsing GTest::gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(test_dsl_parser)
gtest_discover_tests(test_json_parsing)
