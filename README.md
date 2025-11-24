# MAGDA

**Multi Agent Generative DAW Automation**

MAGDA is an AI-powered system that enables natural language control of Digital Audio Workstations (DAWs) through a sophisticated multi-agent architecture.

## Overview

MAGDA translates natural language commands into precise DAW operations, making music production more intuitive and accessible. The system uses specialized AI agents to understand context and execute complex multi-step operations.

## Current Status

This repository is being completely rewritten. The first implementation will be **MAGDA Reaper Edition** - a native C++ extension for Reaper.

## Project Structure

```
magda/
├── reaper/          # Reaper Edition (native C++ extension)
│   ├── src/         # Source code
│   ├── include/     # Headers
│   └── CMakeLists.txt
└── ...
```

## Reaper Edition

The Reaper Edition is a native C++ extension that integrates directly into Reaper, providing:

- Natural language command interpretation
- Context-aware DAW control
- Multi-agent operation execution
- Real-time project state awareness

See [reaper/README.md](reaper/README.md) for build and installation instructions.

## Architecture (Planned)

MAGDA uses a multi-agent architecture where specialized agents handle different aspects of DAW control:

- **Track Agent**: Track creation, naming, routing
- **Volume Agent**: Level control, automation
- **Effect Agent**: Plugin management, parameter control
- **Clip Agent**: Audio/MIDI item manipulation
- **MIDI Agent**: MIDI editing and sequencing

## License

This project is private and proprietary.

## Development

This project is in active development. The codebase is being restructured and rewritten.
