cmake_minimum_required(VERSION 3.22)
project(magda-reaper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reaper SDK path - adjust this to your Reaper SDK location
# The SDK can be obtained from: https://github.com/justinfrankel/reaper-sdk
set(REAPER_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reaper-sdk/sdk" CACHE PATH "Path to Reaper SDK (sdk/ subdirectory)")

# Check if Reaper SDK exists
set(REAPER_SDK_FOUND FALSE)
if(EXISTS "${REAPER_SDK_PATH}/reaper_plugin.h")
    set(REAPER_SDK_FOUND TRUE)
    message(STATUS "Reaper SDK found at ${REAPER_SDK_PATH}")
else()
    message(WARNING "Reaper SDK not found at ${REAPER_SDK_PATH}")
    message(STATUS "To set up the SDK (it's a git submodule):")
    message(STATUS "  git submodule update --init --recursive")
    message(STATUS "  Or set REAPER_SDK_PATH to point to the sdk/ directory")
    message(STATUS "Building without SDK - extension will not compile until SDK is available")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(REAPER_SDK_FOUND)
    include_directories(${REAPER_SDK_PATH})
    # WDL is required by Reaper SDK (separate submodule)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/WDL)
endif()

# Source files
set(SOURCES
    src/main.cpp
)

# Create the extension library
add_library(magda_reaper MODULE ${SOURCES})

# Platform-specific settings
if(APPLE)
    # macOS: .dylib extension (no bundle needed for Reaper extensions)
    set_target_properties(magda_reaper PROPERTIES SUFFIX ".dylib")
elseif(WIN32)
    # Windows: .dll extension
    set_target_properties(magda_reaper PROPERTIES SUFFIX ".dll")
else()
    # Linux: .so extension
    set_target_properties(magda_reaper PROPERTIES SUFFIX ".so")
endif()

# Compiler-specific flags
if(MSVC)
    target_compile_options(magda_reaper PRIVATE /W4)
else()
    target_compile_options(magda_reaper PRIVATE -Wall -Wextra)
endif()

# Installation
install(TARGETS magda_reaper
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Auto-copy to Reaper UserPlugins directory after build (macOS)
if(APPLE)
    set(REAPER_USERPLUGINS_DIR "$ENV{HOME}/Library/Application Support/REAPER/UserPlugins")
    add_custom_command(TARGET magda_reaper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REAPER_USERPLUGINS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:magda_reaper>
            "${REAPER_USERPLUGINS_DIR}/magda_reaper.dylib"
        COMMENT "Copying MAGDA extension to Reaper UserPlugins directory..."
    )
endif()

