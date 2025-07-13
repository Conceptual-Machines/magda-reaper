cmake_minimum_required(VERSION 3.22)
project(magda_cpp VERSION 0.2.1 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find dependencies
find_package(OpenSSL REQUIRED)

# PkgConfig is not available on Windows by default, so make it optional there
if(WIN32)
    find_package(PkgConfig)
    if(NOT PKG_CONFIG_FOUND)
        message(WARNING "PkgConfig not found on Windows - some dependencies may need manual configuration")
    endif()
else()
    find_package(PkgConfig REQUIRED)
endif()

# Let llmcpp handle nlohmann_json to avoid conflicts
# find_package(nlohmann_json REQUIRED)  # Commented out to avoid conflicts

# Add llmcpp library
# Option 1: If llmcpp is installed system-wide
find_package(llmcpp QUIET)
if(NOT llmcpp_FOUND)
    # Option 2: Use FetchContent to get llmcpp from GitHub
    include(FetchContent)
    FetchContent_Declare(
        llmcpp
        GIT_REPOSITORY https://github.com/lucaromagnoli/llmcpp.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(llmcpp)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install configuration
include(GNUInstallDirs)
install(TARGETS magda_cpp
    EXPORT magda_cppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT magda_cppTargets
    FILE magda_cppTargets.cmake
    NAMESPACE magda_cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/magda_cpp
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    magda_cppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/magda_cppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/magda_cpp
)
