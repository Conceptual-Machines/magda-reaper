# MAGDA REAPER

Native REAPER extension for **MAGDA** (**M**ulti-**A**gent **G**enerative **D**AW **A**utomation) - AI-powered DAW control via natural language commands.

## Overview

MAGDA REAPER is a native C++ extension for REAPER that enables natural language control of your DAW. This is the REAPER implementation of the MAGDA system.

## Features

- 🎹 **Natural Language DAW Control** - Create tracks, add effects, manage clips via plain English
- 🎸 **JSFX Generation** - AI-assisted audio effect creation
- 🥁 **Drum Pattern Generation** - Intelligent drum programming
- 🎚️ **Mix Analysis** - AI-powered mixing suggestions
- 🔌 **Plugin Management** - Scan and manage plugins with aliases
- 💬 **Chat Interface** - Modern ImGui-based chat UI

## Requirements

### Runtime Dependencies

- **REAPER** 6.0+ (64-bit recommended)
- **ReaImGui** extension (required for UI) - Install via [ReaPack](https://reapack.com/)

### Build Prerequisites

- REAPER SDK (from GitHub: https://github.com/justinfrankel/reaper-sdk)
- CMake 3.22+
- C++20 compatible compiler

### Setup Reaper SDK and WDL

Both REAPER SDK and WDL are included as git submodules. After cloning this repository, initialize submodules:

```bash
git submodule update --init --recursive
```

Then create a symlink so the SDK can find WDL (required for building):

```bash
cd reaper-sdk
ln -s ../WDL WDL
cd ..
```

The SDK headers will be in `reaper-sdk/sdk/` and WDL will be in `WDL/`

### Build Instructions

**Using Make (Recommended):**

```bash
make build
```

This will automatically:
- Initialize submodules (Reaper SDK and WDL)
- Create the WDL symlink
- Configure and build the extension

**Using CMake directly:**

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

**Other Make commands:**
- `make setup` - Initialize submodules and create symlink
- `make clean` - Remove build directory
- `make rebuild` - Clean and rebuild
- `make help` - Show all available commands

The CMake configuration will automatically find the SDK if it's in `reaper-sdk/sdk/` relative to the project root, or you can set `REAPER_SDK_PATH` to point to the `sdk/` directory.

## Installation

On macOS, the extension is automatically copied to REAPER's UserPlugins directory after a successful build.

For manual installation, copy the built extension to REAPER's `UserPlugins` directory:
- macOS: `~/Library/Application Support/REAPER/UserPlugins/`
- Windows: `%APPDATA%\REAPER\UserPlugins\`
- Linux: `~/.config/REAPER/UserPlugins/`

## Project Structure

```
magda-reaper/
├── src/                    # Source code
├── include/                # Headers
├── CMakeLists.txt         # Build configuration
├── Makefile               # Build convenience script
├── reaper-sdk/            # REAPER SDK (submodule)
└── WDL/                   # WDL library (submodule)
```

## Architecture

This extension integrates with the MAGDA multi-agent system:

- **DSL Parser**: Parses MAGDA DSL code (from `magda-dsl` repository)
- **API Client**: Communicates with `magda-api` backend
- **Action Executor**: Executes REAPER actions from DSL commands
- **State Manager**: Tracks REAPER project state for AI context
- **Plugin Scanner**: Scans installed plugins for AI awareness
- **ReaImGui UI**: Modern immediate-mode GUI for all windows

## Dependencies

### Build Dependencies

- **REAPER SDK**: REAPER plugin API
- **WDL**: REAPER's utility library (included as submodule)
- **magda-dsl**: DSL parser (C++ implementation, included)

### Runtime Dependencies

- **ReaImGui**: Required for UI - install via ReaPack
- **magda-api**: Backend API service (local or hosted)

## Installing ReaImGui

ReaImGui is required for MAGDA's user interface. Install it via ReaPack:

1. Install [ReaPack](https://reapack.com/) if you haven't already
2. In REAPER, go to **Extensions → ReaPack → Browse packages**
3. Search for "ReaImGui" and install it
4. Restart REAPER

## License

AGPL v3 - See [LICENSE](LICENSE) file for details.

## Related Repositories

- [magda-api](https://github.com/Conceptual-Machines/magda-api) - Backend API service (stateless Go API with agents)
- [magda-dsl](https://github.com/Conceptual-Machines/magda-dsl) - DSL specification and parsers
