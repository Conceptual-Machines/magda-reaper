# MAGDA Python Library

**Multi Agent Domain Automation** - Translate natural language prompts into domain-specific commands.

[![Python 3.12+](https://img.shields.io/badge/python-3.12+-blue.svg)](https://www.python.org/downloads/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 🚀 Quick Start

```python
from magda import MAGDAPipeline

# Initialize the pipeline
pipeline = MAGDAPipeline()

# Process a natural language prompt
result = pipeline.process_prompt("Create a new track called 'Bass'")

# Get the generated commands
print(result['commands'])
```

## 🎯 Features

- **Domain-Agnostic Architecture** - Support for multiple domains (DAW, Desktop, Web, etc.)
- **Intelligent Processing** - Semantic complexity detection and model selection
- **Multi-Language Support** - Handle prompts in multiple languages including Japanese
- **Extensible Design** - Easy to add new domains and capabilities

## 📚 Documentation

- **[Quick Start Guide](quickstart.md)** - Get up and running in minutes
- **[API Reference](api/)** - Complete API documentation
- **[Examples](examples/)** - Usage examples and tutorials
- **[Development Guide](development/)** - Contributing and architecture

## 🏗️ Architecture

MAGDA uses a domain-agnostic architecture with:

- **Core Pipeline** - Main processing engine
- **Domain Factories** - Create domain-specific components
- **Agents** - Specialized processors for different operations
- **Complexity Detection** - Intelligent model selection

## 🎵 DAW Domain

Currently supports DAW (Digital Audio Workstation) automation:

- Track management (create, delete, rename)
- Volume control (set levels, mute/unmute)
- Effect processing (reverb, compression, delay, etc.)
- MIDI operations (clips, notes, quantization)
- Clip management (create, move, duplicate)

## 🔧 Installation

```bash
# Install from PyPI (when available)
pip install magda

# Or install from source
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python
pip install -e .
```

## 🤝 Contributing

We welcome contributions! See our [Contributing Guide](development/contributing.md) for details.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

---

**MAGDA** - Making AI-powered domain automation accessible to everyone! 🚀
