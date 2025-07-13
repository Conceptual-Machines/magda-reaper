# Example executable
add_executable(magda_example
    main.cpp
)

# Link with the main library
target_link_libraries(magda_example
    PRIVATE
        magda_cpp
)

# Set target properties
set_target_properties(magda_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
) 