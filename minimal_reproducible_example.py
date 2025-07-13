#!/usr/bin/env python3
"""
Minimal Reproducible Example: o3-mini Temperature Parameter Issue

This script demonstrates the problem where the llmcpp library sends the
temperature parameter to the OpenAI Responses API when using the o3-mini model,
which doesn't support this parameter.

Problem:
- o3-mini is a reasoning model that doesn't support temperature
- llmcpp library includes temperature in the request JSON
- OpenAI API returns: "Unsupported parameter: 'temperature' is not supported with this model"

Expected Behavior:
- temperature parameter should be omitted entirely for o3-mini model
- API call should succeed without errors

To reproduce:
1. Set OPENAI_API_KEY environment variable
2. Run: python minimal_reproducible_example.py
"""

import os


def check_environment():
    """Check if required environment is set up."""
    if not os.getenv("OPENAI_API_KEY"):
        print("❌ Error: OPENAI_API_KEY environment variable not set")
        print("Please set your OpenAI API key:")
        print("export OPENAI_API_KEY='your-api-key-here'")
        return False
    return True


def create_cpp_test_file():
    """Create a minimal C++ test file that reproduces the issue."""
    cpp_code = """
#include <iostream>
#include <string>
#include "openai/OpenAI.hpp"

int main() {
    try {
        // Initialize OpenAI client
        openai::OpenAI client;

        // Create a responses request with o3-mini model
        openai::ResponsesRequest request;
        request.model = "o3-mini";
        request.instructions = "You are a helpful assistant.";
        request.input = "Hello, how are you?";

        // This is the problematic line - temperature should not be sent for o3-mini
        request.temperature = 0.5f;

        std::cout << "Sending request to OpenAI API..." << std::endl;

        // This will fail because temperature is included in the request
        auto response = client.responses.create(request);

        std::cout << "✅ Success! Response: " << response.output_text << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
"""

    with open("test_o3_mini_temperature.cpp", "w") as f:
        f.write(cpp_code)

    print("📝 Created test_o3_mini_temperature.cpp")


def create_python_comparison():
    """Create a Python comparison that shows the correct behavior."""
    python_code = '''
#!/usr/bin/env python3
"""
Python SDK Comparison - Shows correct behavior
"""

import os
import openai
from dotenv import load_dotenv

load_dotenv()

def test_python_sdk():
    """Test Python SDK with o3-mini model."""
    try:
        client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

        print("🐍 Testing Python SDK with o3-mini...")

        # Python SDK correctly omits temperature for o3-mini
        response = client.responses.create(
            model="o3-mini",
            instructions="You are a helpful assistant.",
            input="Hello, how are you?"
            # Note: no temperature parameter - this is correct!
        )

        print(f"✅ Python SDK Success! Response: {response.output_text}")
        return True

    except Exception as e:
        print(f"❌ Python SDK Error: {e}")
        return False

def test_python_sdk_with_temperature():
    """Test Python SDK with temperature parameter (should fail)."""
    try:
        client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

        print("🐍 Testing Python SDK with o3-mini + temperature (should fail)...")

        # This should fail because o3-mini doesn't support temperature
        response = client.responses.create(
            model="o3-mini",
            instructions="You are a helpful assistant.",
            input="Hello, how are you?",
            temperature=0.5  # This should cause an error
        )

        print(f"❌ Unexpected success! Response: {response.output_text}")
        return False

    except Exception as e:
        print(f"✅ Expected error: {e}")
        return True

if __name__ == "__main__":
    print("=" * 60)
    print("PYTHON SDK COMPARISON")
    print("=" * 60)

    success1 = test_python_sdk()
    success2 = test_python_sdk_with_temperature()

    if success1 and success2:
        print("\\n✅ Python SDK behaves correctly!")
    else:
        print("\\n❌ Python SDK has issues!")
'''

    with open("test_python_comparison.py", "w") as f:
        f.write(python_code)

    print("📝 Created test_python_comparison.py")


def create_build_script():
    """Create a build script for the C++ test."""
    build_script = """#!/bin/bash
# Build script for the C++ test

echo "🔨 Building C++ test..."

# Check if we're in the right directory
if [ ! -f "test_o3_mini_temperature.cpp" ]; then
    echo "❌ Error: test_o3_mini_temperature.cpp not found"
    echo "Please run this script from the directory containing the test file"
    exit 1
fi

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
)

for path in "${PATHS[@]}"; do
    if [ -d "$path" ]; then
        if [ -f "$path/openai/OpenAI.hpp" ]; then
            LLMCPP_INCLUDE="$path"
        fi
        if [ -f "$path/libopenai.a" ] || [ -f "$path/libopenai.so" ] || [ -f "$path/libopenai.dylib" ]; then
            LLMCPP_LIB="$path"
        fi
    fi
done

if [ -z "$LLMCPP_INCLUDE" ]; then
    echo "❌ Error: Could not find llmcpp headers"
    echo "Please install llmcpp library first"
    exit 1
fi

if [ -z "$LLMCPP_LIB" ]; then
    echo "❌ Error: Could not find llmcpp library"
    echo "Please install llmcpp library first"
    exit 1
fi

echo "📁 Found llmcpp headers at: $LLMCPP_INCLUDE"
echo "📁 Found llmcpp library at: $LLMCPP_LIB"

# Build the test
g++ -std=c++17 -I"$LLMCPP_INCLUDE" test_o3_mini_temperature.cpp -L"$LLMCPP_LIB" -lopenai -o test_o3_mini_temperature

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "Run with: ./test_o3_mini_temperature"
else
    echo "❌ Build failed!"
    exit 1
fi
"""

    with open("build_test.sh", "w") as f:
        f.write(build_script)

    # Make it executable
    os.chmod("build_test.sh", 0o755)
    print("📝 Created build_test.sh")


def create_readme():
    """Create a README explaining the issue and how to reproduce it."""
    readme = """# Minimal Reproducible Example: o3-mini Temperature Parameter Issue

## Problem Description

The llmcpp library incorrectly sends the `temperature` parameter to the OpenAI Responses API when using the `o3-mini` model, which doesn't support this parameter. This causes API calls to fail with:

```
Unsupported parameter: 'temperature' is not supported with this model.
```

## Files in this Example

- `test_o3_mini_temperature.cpp` - C++ test that reproduces the issue
- `test_python_comparison.py` - Python comparison showing correct behavior
- `build_test.sh` - Script to build the C++ test
- `minimal_reproducible_example.py` - This script that creates everything

## How to Reproduce

### Prerequisites

1. Set your OpenAI API key:
   ```bash
   export OPENAI_API_KEY='your-api-key-here'
   ```

2. Install llmcpp library (if not already installed)

### Steps

1. **Create the test files:**
   ```bash
   python minimal_reproducible_example.py
   ```

2. **Test Python SDK (should work):**
   ```bash
   python test_python_comparison.py
   ```

3. **Build and test C++ version (should fail):**
   ```bash
   chmod +x build_test.sh
   ./build_test.sh
   ./test_o3_mini_temperature
   ```

## Expected Results

### Python SDK (Correct Behavior)
- ✅ `o3-mini` without temperature: **SUCCESS**
- ✅ `o3-mini` with temperature: **FAILS** (as expected)

### C++ SDK (Current Buggy Behavior)
- ❌ `o3-mini` with temperature: **FAILS** (but shouldn't send temperature at all)

## Root Cause

The issue is in the llmcpp library's `ResponsesRequest::toJson()` method:

```cpp
// Current problematic code:
if (temperature) j["temperature"] = *temperature;

// Should be:
if (temperature && supportsTemperature()) j["temperature"] = *temperature;
```

## Solution

The llmcpp library needs to:

1. Add a `supportsTemperature()` method that checks if the model supports temperature
2. Only include temperature in the JSON if the model supports it
3. For `o3-mini`, `o3`, `o1-mini`, `o1`, `o4-mini`, `o4` models, temperature should be omitted entirely

## Models That Don't Support Temperature

- `o3-mini` (reasoning model)
- `o3` (reasoning model)
- `o1-mini` (reasoning model)
- `o1` (reasoning model)
- `o4-mini` (reasoning model)
- `o4` (reasoning model)

## Models That Support Temperature

- `gpt-4o-mini`
- `gpt-4o`
- `gpt-4-turbo`
- `claude-3-5-sonnet`
- `claude-3-opus`
- And other completion models

## API Documentation Reference

According to OpenAI's API documentation, the `temperature` parameter is not supported for reasoning models like `o3-mini`. The Python SDK correctly handles this by using `NotGiven` to omit the parameter entirely from the request.

## Impact

This bug prevents the use of reasoning models (o3-mini, o3, o1-mini, etc.) in applications using the llmcpp library, forcing developers to use completion models instead or implement workarounds.
"""

    with open("README.md", "w") as f:
        f.write(readme)

    print("📝 Created README.md")


def main():
    """Main function to create the minimal reproducible example."""
    print("🔧 Creating Minimal Reproducible Example")
    print("=" * 50)

    if not check_environment():
        return

    print("✅ Environment check passed")

    # Create all the test files
    create_cpp_test_file()
    create_python_comparison()
    create_build_script()
    create_readme()

    print("\n" + "=" * 50)
    print("✅ Minimal reproducible example created!")
    print("\n📁 Files created:")
    print("  - test_o3_mini_temperature.cpp")
    print("  - test_python_comparison.py")
    print("  - build_test.sh")
    print("  - README.md")

    print("\n🚀 Next steps:")
    print("1. Test Python SDK: python test_python_comparison.py")
    print("2. Build C++ test: ./build_test.sh")
    print("3. Run C++ test: ./test_o3_mini_temperature")
    print("\n📖 See README.md for detailed instructions")


if __name__ == "__main__":
    main()
