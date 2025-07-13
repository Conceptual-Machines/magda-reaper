# Download and link Catch2
include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(catch2)

# Test source files
set(TEST_SOURCES
    test_agents.cpp
    test_pipeline.cpp
    test_integration.cpp
)

# Create test executable with all tests
add_executable(magda_cpp_tests
    ${TEST_SOURCES}
)

# Link against the library and test framework
target_link_libraries(magda_cpp_tests PRIVATE
    magda_cpp
    Catch2::Catch2WithMain
)

# Include directories
target_include_directories(magda_cpp_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/utils
)

# Add test to ctest
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(magda_cpp_tests)
