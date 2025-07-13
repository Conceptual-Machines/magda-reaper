#!/bin/bash

# Build and test script for minimal reproducible example
# Demonstrates the o3-mini temperature parameter issue in llmcpp library

set -e

echo "🔧 Building minimal reproducible example..."
echo "This demonstrates the o3-mini temperature parameter bug in llmcpp library"
echo ""

# Check if we're in the right directory
if [ ! -f "cpp_minimal_reproducible_example.cpp" ]; then
    echo "❌ Error: cpp_minimal_reproducible_example.cpp not found"
    echo "Please run this script from the directory containing the test file"
    exit 1
fi

# Check for required environment variable
if [ -z "$OPENAI_API_KEY" ]; then
    echo "❌ Error: OPENAI_API_KEY environment variable not set"
    echo "Please set your OpenAI API key:"
    echo "export OPENAI_API_KEY='your-api-key-here'"
    exit 1
fi

echo "✅ Environment check passed"
echo ""

# Try to find llmcpp installation
LLMCPP_INCLUDE=""
LLMCPP_LIB=""

# Common installation paths
PATHS=(
    "/usr/local/include"
    "/usr/include"
    "/opt/homebrew/include"
    "/usr/local/lib"
    "/usr/lib"
    "/opt/homebrew/lib"
    "/usr/local/lib64"
    "/usr/lib64"
)

echo "🔍 Searching for llmcpp library..."

for path in "${PATHS[@]}"; do
    if [ -d "$path" ]; then
        if [ -f "$path/openai/OpenAI.hpp" ]; then
            LLMCPP_INCLUDE="$path"
            echo "📁 Found llmcpp headers at: $path"
        fi
        if [ -f "$path/libopenai.a" ] || [ -f "$path/libopenai.so" ] || [ -f "$path/libopenai.dylib" ]; then
            LLMCPP_LIB="$path"
            echo "📁 Found llmcpp library at: $path"
        fi
    fi
done

if [ -z "$LLMCPP_INCLUDE" ]; then
    echo "❌ Error: Could not find llmcpp headers"
    echo "Please install llmcpp library first:"
    echo "  git clone https://github.com/your-username/llmcpp.git"
    echo "  cd llmcpp && mkdir build && cd build"
    echo "  cmake .. && make && sudo make install"
    exit 1
fi

if [ -z "$LLMCPP_LIB" ]; then
    echo "❌ Error: Could not find llmcpp library"
    echo "Please install llmcpp library first"
    exit 1
fi

echo ""
echo "🔨 Building test executable..."

# Build the test
g++ -std=c++17 \
    -I"$LLMCPP_INCLUDE" \
    cpp_minimal_reproducible_example.cpp \
    -L"$LLMCPP_LIB" \
    -lopenai \
    -o test_o3_mini_temperature

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo ""
    echo "🚀 Running test..."
    echo "Expected: Should fail with 'Unsupported parameter: temperature' error"
    echo "This demonstrates the bug in llmcpp library"
    echo ""

    # Run the test
    ./test_o3_mini_temperature

    echo ""
    echo "📋 Summary:"
    echo "  - The test failed because llmcpp library sends temperature parameter"
    echo "  - o3-mini model doesn't support temperature parameter"
    echo "  - This is a bug in the llmcpp library that needs to be fixed"
    echo ""
    echo "🛠️  To fix this, the llmcpp library needs to:"
    echo "  1. Add a supportsTemperature() method"
    echo "  2. Check if model supports temperature before including it"
    echo "  3. For reasoning models (o3-mini, o3, o1-mini, etc.): omit temperature"

else
    echo "❌ Build failed!"
    echo ""
    echo "💡 Troubleshooting tips:"
    echo "  - Make sure llmcpp library is properly installed"
    echo "  - Check that headers and library files are in standard locations"
    echo "  - Try installing with: sudo make install"
    exit 1
fi
