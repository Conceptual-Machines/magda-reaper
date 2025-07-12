# Create the main library
add_library(magda_cpp_lib
    agent.cpp
)

# Add include directories
target_include_directories(magda_cpp_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Create the main executable
add_executable(magda_cpp main.cpp)

target_link_libraries(magda_cpp PRIVATE magda_cpp_lib) 