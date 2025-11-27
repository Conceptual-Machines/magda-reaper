# MAGDA REAPER

Native REAPER extension for MAGDA (Musical AI Digital Assistant) - AI-powered DAW control via natural language commands.

## Overview

MAGDA REAPER is a native C++ extension for REAPER that enables natural language control of your DAW. This is the REAPER implementation of the MAGDA system.

## Features

- Natural language commands to control REAPER
- Track creation and management
- Plugin management and scanning
- Chat interface for commands
- Context-aware state management
- Plugin autocomplete (coming soon)
- Sample browser (coming soon)

## Building

### Prerequisites

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
- **Action Executor**: Executes REAPER actions
- **State Manager**: Tracks REAPER project state
- **Plugin Scanner**: Scans installed plugins

## Dependencies

- **magda-dsl**: DSL parser (C++ implementation)
- **magda-api**: Backend API service
- **REAPER SDK**: REAPER plugin API
- **WDL**: REAPER's utility library

## License

AGPL v3 - See [LICENSE](LICENSE) file for details.

## Related Repositories

- [magda-dsl](https://github.com/Conceptual-Machines/magda-dsl) - DSL specification and parsers
- [magda-agents](https://github.com/Conceptual-Machines/magda-agents) - Agent framework
- [magda-api](https://github.com/Conceptual-Machines/magda-api) - Backend API (private)
