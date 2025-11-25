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
    # Include WDL root so reaper_plugin.h can find ../WDL/swell/swell.h
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/WDL)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/magda_chat_window.cpp
)

# Add SWELL stub library to initialize SWELL function pointers
# This is required for SWELL functions to work at runtime
# SWS does this same thing - they link against swell-modstub
if(APPLE)
    set(SWELL_STUB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL/swell/swell-modstub.mm)
else()
    set(SWELL_STUB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL/swell/swell-modstub-generic.cpp)
endif()

# Create SWELL stub library
add_library(swell_stub STATIC ${SWELL_STUB_SRC})
target_compile_definitions(swell_stub PUBLIC SWELL_PROVIDED_BY_APP)
target_include_directories(swell_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/WDL)

if(APPLE)
    find_library(APPKIT_LIB AppKit)
    target_link_libraries(swell_stub PUBLIC ${APPKIT_LIB})
endif()

# Create the extension library
# Reaper extensions must be named reaper_<name>.dylib
add_library(reaper_magda MODULE ${SOURCES})

# Link against SWELL stub to initialize function pointers
target_link_libraries(reaper_magda PRIVATE swell_stub)

# Platform-specific settings
if(APPLE)
    # macOS: .dylib extension (no bundle needed for Reaper extensions)
    set_target_properties(reaper_magda PROPERTIES SUFFIX ".dylib")
elseif(WIN32)
    # Windows: .dll extension
    set_target_properties(reaper_magda PROPERTIES SUFFIX ".dll")
else()
    # Linux: .so extension
    set_target_properties(reaper_magda PROPERTIES SUFFIX ".so")
endif()

# Compiler-specific flags
if(MSVC)
    target_compile_options(reaper_magda PRIVATE /W4)
else()
    target_compile_options(reaper_magda PRIVATE -Wall -Wextra)
    # Allow undefined symbols for SWELL function pointers (provided by Reaper at runtime)
    target_link_options(reaper_magda PRIVATE -undefined dynamic_lookup)
endif()

# Installation
install(TARGETS reaper_magda
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Auto-copy to Reaper UserPlugins directory after build (macOS)
if(APPLE)
    set(REAPER_USERPLUGINS_DIR "$ENV{HOME}/Library/Application Support/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_magda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REAPER_USERPLUGINS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:reaper_magda>
            "${REAPER_USERPLUGINS_DIR}/reaper_magda.dylib"
        COMMENT "Copying MAGDA extension to Reaper UserPlugins directory..."
    )
endif()

