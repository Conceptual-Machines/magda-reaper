# MAGDA C++ Implementation

A modern C++20 implementation of MAGDA (Multi Agent Generative DAW API) that translates natural language prompts into generic DAW commands using Large Language Models.

## Features

- **🚀 Modern C++20**: Uses latest C++ features and standard library
- **🤖 LLM Integration**: Powered by [llmcpp](https://github.com/lucaromagnoli/llmcpp) library
- **📝 Structured Outputs**: Uses JsonSchemaBuilder for type-safe LLM responses
- **🎯 Multi-Agent Architecture**: Modular agent system for different DAW operations
- **🌐 Cross-platform**: Works on Linux, macOS, and Windows
- **📦 CMake Integration**: Easy to integrate into existing projects

## Quick Start

### Prerequisites

- CMake 3.22+
- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- OpenSSL (for HTTPS support)
- nlohmann/json

### Building

```bash
# Clone the repository
git clone https://github.com/lucaromagnoli/magda.git
cd magda/cpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run the example
./examples/magda_example
```

### Basic Usage

```cpp
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"

int main() {
    // Set your OpenAI API key
    setenv("OPENAI_API_KEY", "your-api-key-here", 1);

    // Create agents
    magda::TrackAgent track_agent;
    magda::VolumeAgent volume_agent;

    // Create a track
    auto track_response = track_agent.execute("create a bass track with Serum");
    std::cout << "DAW Command: " << track_response.daw_command << std::endl;

    // Set volume
    auto volume_response = volume_agent.execute("set volume to 75%");
    std::cout << "DAW Command: " << volume_response.daw_command << std::endl;

    return 0;
}
```

## Architecture

### Core Components

- **`BaseAgent`**: Abstract base class for all agents
- **`TrackAgent`**: Handles track creation operations
- **`VolumeAgent`**: Handles volume, pan, and mute operations
- **`Pipeline`**: Coordinates multiple agents
- **Models**: C++ equivalents of Python Pydantic models

### Agent System

Each agent:
1. **Parses** natural language using LLM with structured JSON schemas
2. **Executes** the operation and generates results
3. **Generates** DAW commands for the operation
4. **Stores** state for future reference

### LLM Integration

The implementation uses the [llmcpp](https://github.com/lucaromagnoli/llmcpp) library which provides:

- **OpenAI Responses API** support
- **JsonSchemaBuilder** for structured outputs
- **Type-safe model selection**
- **Async operations** with std::future
- **Error handling** and usage tracking

## Available Agents

### TrackAgent

Handles track creation operations:

```cpp
magda::TrackAgent agent;

// Supported operations:
agent.execute("create a bass track with Serum");
agent.execute("add a drum track");
agent.execute("create track for lead synth");
```

**Capabilities:**
- Track creation with VST plugins
- Audio and MIDI track types
- Track naming and identification

### VolumeAgent

Handles volume control operations:

```cpp
magda::VolumeAgent agent;

// Supported operations:
agent.execute("set volume to 75%");
agent.execute("pan the track to the left");
agent.execute("mute the bass track");
```

**Capabilities:**
- Volume level control (0-100%)
- Pan positioning (-1.0 to 1.0)
- Mute/unmute tracks

## Structured Outputs with JsonSchemaBuilder

The implementation leverages llmcpp's JsonSchemaBuilder for type-safe LLM responses:

```cpp
// Example: Track parameters schema
auto schema = llmcpp::JsonSchemaBuilder()
    .type("object")
    .title("Track Parameters")
    .description("Parameters for creating a track in a DAW")
    .property("name", llmcpp::JsonSchemaBuilder()
        .type("string")
        .description("The name of the track")
        .required())
    .property("vst", llmcpp::JsonSchemaBuilder()
        .type("string")
        .description("The VST plugin name")
        .optionalString())
    .property("type", llmcpp::JsonSchemaBuilder()
        .type("string")
        .enumValues({"audio", "midi"})
        .defaultValue("midi"))
    .required({"name"});
```

This ensures:
- **Type safety** at compile time
- **Validation** of LLM responses
- **Consistent** data structures
- **Better error handling**

## Pipeline Usage

Coordinate multiple agents using the Pipeline class:

```cpp
// Create a pipeline with multiple agents
auto pipeline = magda::Pipeline::createDefaultPipeline();

// Process operations - pipeline finds the right agent
auto response1 = pipeline.processOperation("create a bass track");
auto response2 = pipeline.processOperation("set volume to 80%");
auto response3 = pipeline.processOperation("pan to center");
```

## Integration

### As a Library

```cmake
# In your CMakeLists.txt
find_package(magda_cpp REQUIRED)
target_link_libraries(your_target PRIVATE magda_cpp::magda_cpp)
```

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    magda_cpp
    GIT_REPOSITORY https://github.com/lucaromagnoli/magda.git
    GIT_TAG main
)
FetchContent_MakeAvailable(magda_cpp)
target_link_libraries(your_target PRIVATE magda_cpp)
```

## Configuration

### Environment Variables

- `OPENAI_API_KEY`: Your OpenAI API key (required)

### API Key Setup

```cpp
// Option 1: Environment variable (recommended)
setenv("OPENAI_API_KEY", "your-key", 1);
magda::TrackAgent agent;

// Option 2: Direct parameter
magda::TrackAgent agent("your-api-key");
```

## Examples

See the `examples/` directory for complete working examples:

- `main.cpp`: Multi-agent system demonstration
- Shows track creation and volume control
- Demonstrates agent capabilities and error handling

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new features
5. Ensure all tests pass
6. Submit a pull request

## License

Licensed under the MIT License. See LICENSE for details.

## Dependencies

- [llmcpp](https://github.com/lucaromagnoli/llmcpp): LLM API library
- nlohmann/json: JSON parsing and manipulation
- OpenSSL: HTTPS support
- CMake: Build system
