# MAGDA Reaper Edition

Native Reaper extension for MAGDA (Multi Agent Generative DAW Automation) - AI-powered DAW control via natural language commands.

## Overview

MAGDA Reaper Edition is a native C++ extension for Reaper that enables natural language control of your DAW. This is the first implementation of the MAGDA system, focused on Reaper.

## Features

- Natural language commands to control Reaper
- Track creation and management
- Plugin management
- Mix analysis and feedback (future)
- Simple chat UI (future)

## Building

### Prerequisites

- Reaper SDK (from GitHub: https://github.com/justinfrankel/reaper-sdk)
- CMake 3.22+
- C++20 compatible compiler

### Setup Reaper SDK and WDL

Both Reaper SDK and WDL are included as git submodules. After cloning this repository, initialize submodules:

```bash
cd reaper
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

```bash
cd reaper
mkdir build
cd build
cmake ..
cmake --build .
```

The CMake configuration will automatically find the SDK if it's in `reaper-sdk/sdk/` relative to the project root, or you can set `REAPER_SDK_PATH` to point to the `sdk/` directory.

## Installation

On macOS, the extension is automatically copied to Reaper's UserPlugins directory after a successful build.

For manual installation, copy the built extension to Reaper's `UserPlugins` directory:
- macOS: `~/Library/Application Support/REAPER/UserPlugins/`
- Windows: `%APPDATA%\REAPER\UserPlugins\`
- Linux: `~/.config/REAPER/UserPlugins/`

## Development

The extension uses:
- Reaper SDK for DAW control
- llmcpp for LLM integration (to be integrated)
- Reaper's UI framework for the interface

## Architecture

This extension will integrate with the MAGDA multi-agent system to provide:
- Context-aware command interpretation
- Specialized agents for different DAW operations
- Natural language to Reaper API translation

