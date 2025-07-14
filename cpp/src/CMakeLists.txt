# Create the main library target
add_library(magda_cpp
    models.cpp
    agents/base_agent.cpp
    agents/track_agent.cpp
    agents/volume_agent.cpp
    agents/effect_agent.cpp
    agents/midi_agent.cpp
    agents/clip_agent.cpp
    agents/operation_identifier.cpp
    pipeline.cpp
    ../../shared/utils/prompt_loader.cpp
)

# Generate binary data headers from prompt files only
set(PROMPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/prompts)

# Read prompt files and convert to C++ string literals
file(GLOB PROMPT_FILES ${PROMPTS_DIR}/*.txt)

# Create binary data variables
set(BINARY_DATA_CONTENT "")

# Process prompt files
foreach(PROMPT_FILE ${PROMPT_FILES})
    get_filename_component(PROMPT_NAME ${PROMPT_FILE} NAME_WE)
    file(READ ${PROMPT_FILE} PROMPT_CONTENT)
    string(APPEND BINARY_DATA_CONTENT "
// ${PROMPT_NAME}.txt
static const char* ${PROMPT_NAME}_prompt_data = R\"(${PROMPT_CONTENT})\";
static const size_t ${PROMPT_NAME}_prompt_size = sizeof(${PROMPT_NAME}_prompt_data) - 1;
")
endforeach()

# Generate the binary data header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/utils/binary_data.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/binary_data.h
    @ONLY
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Create the main executable
add_executable(magda_cpp_cli main.cpp)

# Link against the library
target_link_libraries(magda_cpp_cli PRIVATE magda_cpp)
