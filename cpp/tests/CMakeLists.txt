# Find GTest
find_package(GTest REQUIRED)

# Create test executable
add_executable(magda_cpp_tests
    test_agents.cpp
    test_pipeline.cpp
)

# Link against the library and GTest
target_link_libraries(magda_cpp_tests PRIVATE
    magda_cpp_lib
    GTest::gtest
    GTest::gtest_main
)

# Add tests
add_test(NAME magda_cpp_tests COMMAND magda_cpp_tests)
