# MAGDA REAPER

Native REAPER extension for **MAGDA** (**M**ulti-**A**gent **G**enerative **D**AW **A**utomation) - AI-powered DAW control via natural language commands.

## Overview

MAGDA REAPER is a native C++ extension for REAPER that enables natural language control of your DAW. Talk to your DAW using plain English to create tracks, add effects, generate drum patterns, and more.

## Features

- 🎹 **Natural Language DAW Control** - Create tracks, add effects, manage clips via plain English
- 🎸 **JSFX Generation** - AI-assisted audio effect creation with live preview
- 🥁 **Drum Pattern Generation** - Intelligent drum programming with grid notation
- 🎚️ **Mix Analysis** - AI-powered mixing suggestions based on your project
- 🔌 **Plugin Management** - Scan and manage plugins with aliases
- 💬 **Chat Interface** - Modern ImGui-based chat UI
- 🔄 **Streaming Responses** - Real-time streaming from AI for instant feedback

## Requirements

### Runtime Dependencies

- **REAPER** 6.0+ (64-bit recommended)
- **ReaImGui** extension (required for UI) - Install via [ReaPack](https://reapack.com/)
- **OpenAI API Key** (for AI features)

### Build Prerequisites

- REAPER SDK (included as submodule)
- CMake 3.22+
- C++20 compatible compiler
- libcurl (macOS/Linux)

## Quick Start

### 1. Clone and Setup

```bash
git clone --recursive https://github.com/Conceptual-Machines/magda-reaper.git
cd magda-reaper
```

### 2. Build

```bash
make build
```

This will automatically:
- Initialize submodules (REAPER SDK and WDL)
- Configure and build the extension
- Copy to REAPER's UserPlugins directory (macOS)

### 3. Configure

Create a `.env` file in the project root with your OpenAI API key:

```
OPENAI_API_KEY=sk-your-key-here
```

Or set the API key in REAPER via **Extensions → MAGDA → Settings**.

### 4. Install ReaImGui

ReaImGui is required for MAGDA's user interface:

1. Install [ReaPack](https://reapack.com/) if you haven't already
2. In REAPER, go to **Extensions → ReaPack → Browse packages**
3. Search for "ReaImGui" and install it
4. Restart REAPER

## Build Instructions

**Using Make (Recommended):**

```bash
make build      # Build the extension
make clean      # Remove build directory
make rebuild    # Clean and rebuild
make help       # Show all available commands
```

**Using CMake directly:**

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Installation

On macOS, the extension is automatically copied to REAPER's UserPlugins directory after a successful build.

For manual installation, copy the built extension to REAPER's `UserPlugins` directory:
- macOS: `~/Library/Application Support/REAPER/UserPlugins/`
- Windows: `%APPDATA%\REAPER\UserPlugins\`
- Linux: `~/.config/REAPER/UserPlugins/`

## Project Structure

```
magda-reaper/
├── src/
│   ├── core/           # Main extension entry, state management
│   ├── ui/             # ImGui windows (chat, settings, JSFX editor)
│   ├── api/            # OpenAI client, HTTP handling
│   ├── dsl/            # DSL parser and interpreters
│   ├── analysis/       # Mix analysis, DSP
│   └── plugins/        # Plugin scanning, drum mapping
├── include/            # Public headers
├── tests/
│   ├── unit/           # Unit tests (CI-friendly)
│   └── *.lua           # Integration tests (require REAPER)
├── WDL/                # WDL library (submodule)
└── reaper-sdk/         # REAPER SDK (submodule)
```

## Testing

### Unit Tests (CI)

Unit tests run without REAPER and are executed in GitHub Actions:

```bash
./tests/run_unit_tests.sh
```

### Integration Tests (Local)

Integration tests require REAPER to be installed:

```bash
./tests/run_tests.sh
```

## Architecture

MAGDA uses a multi-agent system with specialized agents for different tasks:

- **DAW Agent** - General DAW control (tracks, clips, effects)
- **Drummer Agent** - Drum pattern generation
- **Arranger Agent** - Song arrangement and structure
- **JSFX Agent** - Audio effect code generation
- **Mix Agent** - Mix analysis and suggestions

Each agent uses a domain-specific DSL that gets parsed and executed as REAPER actions.

## License

AGPL v3 - See [LICENSE](LICENSE) file for details.
