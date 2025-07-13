# Create the main library target
add_library(magda_cpp
    models.cpp
    agents/base_agent.cpp
    agents/track_agent.cpp
    agents/volume_agent.cpp
    # agents/effect_agent.cpp
    # agents/midi_agent.cpp
    # agents/clip_agent.cpp
    agents/operation_identifier.cpp
    pipeline.cpp
    ../../shared/utils/prompt_loader.cpp
)

# Set target properties
set_target_properties(magda_cpp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Link dependencies
target_link_libraries(magda_cpp
    PUBLIC
        llmcpp
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Include directories
target_include_directories(magda_cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../shared/utils>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Create the main executable
add_executable(magda_cpp_cli main.cpp)

# Link against the library
target_link_libraries(magda_cpp_cli PRIVATE magda_cpp)
