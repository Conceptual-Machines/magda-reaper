# MAGDA Python Library

[![Python 3.12+](https://img.shields.io/badge/python-3.12+-blue.svg)](https://www.python.org/downloads/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Code style: ruff](https://img.shields.io/badge/code%20style-ruff-000000.svg)](https://github.com/astral-sh/ruff)

**Multi Agent Domain Automation** - Translate natural language prompts into domain-specific commands.

## 🚀 Quick Start

### Installation

```bash
# Install from PyPI (when available)
pip install magda

# Or install from source
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python
pip install -e .
```

### Basic Usage

```python
from magda import MAGDAPipeline

# Initialize the pipeline
pipeline = MAGDAPipeline()

# Process a natural language prompt
result = pipeline.process_prompt("Create a new track called 'Bass'")

# Get the generated commands
print(result['commands'])
# Output: ['create_track("Bass")']
```

## 🎯 Features

### **Domain-Agnostic Architecture**
- **Multi-domain support**: Currently supports DAW (Digital Audio Workstation) automation
- **Extensible design**: Easy to add new domains (image editing, code generation, etc.)
- **Factory pattern**: Clean separation between domain-specific and generic components

### **Intelligent Processing**
- **Semantic complexity detection**: Uses sentence transformers for accurate task classification
- **Fast-path routing**: Optimized handling for simple operations
- **Multi-language support**: Handles prompts in multiple languages including Japanese
- **Model selection**: Automatically chooses the best AI model based on task complexity

### **DAW Domain Features**
- **Track management**: Create, delete, rename tracks
- **Volume control**: Set levels, mute/unmute, adjust volumes
- **Effect processing**: Add reverb, compression, delay, chorus, EQ
- **MIDI operations**: Create clips, add notes, quantize, transpose
- **Clip management**: Create, move, duplicate, split clips

## 📚 API Reference

### Core Classes

#### `MAGDAPipeline`
Main entry point for processing prompts.

```python
from magda import MAGDAPipeline

pipeline = MAGDAPipeline()
result = pipeline.process_prompt("your prompt here")
```

#### `DomainPipeline`
Base class for domain-agnostic processing.

```python
from magda import DomainPipeline, DAWFactory

factory = DAWFactory()
pipeline = DomainPipeline(factory)
result = pipeline.process_prompt("your prompt here")
```

#### `DAWFactory`
Factory for creating DAW-specific components.

```python
from magda import DAWFactory

factory = DAWFactory()
context = factory.create_context()
orchestrator = factory.create_orchestrator()
agents = factory.create_agents()
```

### Complexity Detection

```python
from magda import get_complexity_detector

detector = get_complexity_detector()
result = detector.detect_complexity("your prompt")
print(f"Complexity: {result.complexity} (confidence: {result.confidence})")
```

## 🧪 Examples

### Basic DAW Operations

```python
from magda import MAGDAPipeline

pipeline = MAGDAPipeline()

# Track operations
result = pipeline.process_prompt("Create a new track called 'Guitar'")
result = pipeline.process_prompt("Delete the drums track")

# Volume operations
result = pipeline.process_prompt("Set guitar volume to -6dB")
result = pipeline.process_prompt("Mute the bass track")

# Effect operations
result = pipeline.process_prompt("Add reverb to the vocals")
result = pipeline.process_prompt("Add compression to the drums")

# MIDI operations
result = pipeline.process_prompt("Create a 4-bar clip with C major chord")
result = pipeline.process_prompt("Quantize the MIDI notes to 16th notes")
```

### Multi-Language Support

```python
# Japanese prompts
result = pipeline.process_prompt("トラック「ベース」の音量を-6dBに設定してください")
# Translation: "Please set the volume of track 'Bass' to -6dB"

# Spanish prompts
result = pipeline.process_prompt("Crear una nueva pista llamada 'Guitarra'")
# Translation: "Create a new track called 'Guitar'"
```

### Domain-Agnostic Usage

```python
from magda import DomainPipeline, DAWFactory

# Create a DAW domain factory
daw_factory = DAWFactory()

# Create a domain-agnostic pipeline
pipeline = DomainPipeline(daw_factory)

# Process prompts
result = pipeline.process_prompt("Create a new track")
```

## 🏗️ Development

### Project Structure

```
python/
├── magda/                    # Core library
│   ├── __init__.py          # Public API exports
│   ├── pipeline.py          # Main pipeline implementation
│   ├── core/                # Domain-agnostic abstractions
│   │   ├── domain.py        # Domain interfaces
│   │   └── pipeline.py      # Base pipeline
│   ├── domains/             # Domain-specific implementations
│   │   └── daw/            # DAW domain
│   │       ├── daw_agents.py
│   │       └── daw_factory.py
│   ├── complexity/          # Complexity detection
│   │   └── semantic_detector.py
│   └── agents/              # Agent implementations
├── examples/                # Usage examples
├── benchmarks/              # Performance testing
├── scripts/                 # Utility scripts
├── tests/                   # Test suite
└── pyproject.toml          # Package configuration
```

### Setup Development Environment

```bash
# Clone the repository
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python

# Install dependencies
uv sync

# Install in development mode
pip install -e .

# Run tests
pytest tests/

# Run linting
ruff check .

# Build package
python build.py
```

### Running Examples

```bash
# Basic usage example
python examples/basic_usage.py

# Domain-agnostic example
python examples/example_domain_agnostic.py

# CLI usage
magda "Create a new track called 'Bass'"
```

## 🔧 Configuration

### Environment Variables

Create a `.env` file in the project root:

```env
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_BASE_URL=https://api.openai.com/v1  # Optional: for custom endpoints
```

### Model Selection

MAGDA automatically selects the best model based on:
- **Task complexity**: Simple, medium, or complex
- **Reasoning requirements**: Whether the task needs logical reasoning
- **Operation type**: Track, volume, effect, clip, or MIDI operations

Available models:
- `gpt-4o-mini`: Fast, cost-effective for simple tasks
- `gpt-4o`: Balanced performance for medium complexity
- `gpt-4o-2024-07-18`: Best performance for complex reasoning tasks

## 📊 Performance

### Complexity Detection Accuracy
- **Semantic approach**: ~95% accuracy using sentence transformers
- **Fallback approach**: ~80% accuracy using word-count heuristics

### Processing Speed
- **Fast-path routing**: <10ms for simple operations
- **Semantic analysis**: ~100-500ms depending on model size
- **Full pipeline**: 1-5 seconds depending on complexity

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes and add tests
4. Run the test suite: `pytest tests/`
5. Commit your changes: `git commit -m 'Add amazing feature'`
6. Push to the branch: `git push origin feature/amazing-feature`
7. Open a Pull Request

### Adding New Domains

To add support for a new domain (e.g., image editing):

1. Create a new domain directory: `magda/domains/image/`
2. Implement domain-specific agents and factory
3. Add domain examples and tests
4. Update documentation

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- OpenAI for providing the GPT models
- Sentence Transformers for semantic analysis capabilities
- The open-source community for inspiration and tools

---

**MAGDA** - Making AI-powered domain automation accessible to everyone! 🚀
