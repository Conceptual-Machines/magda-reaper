# Create the main library
add_library(magda_cpp_lib INTERFACE)

# Add include directories
target_include_directories(magda_cpp_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Add source files (to be added as we implement)
# target_sources(magda_cpp_lib INTERFACE
#     agent.cpp
#     pipeline.cpp
# )

# Create the main executable
add_executable(magda_cpp main.cpp)

# Link against the library
target_link_libraries(magda_cpp PRIVATE magda_cpp_lib) 