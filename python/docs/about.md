# About MAGDA

**Multi Agent Domain Automation** - A powerful, domain-agnostic system for translating natural language into automated actions.

## What is MAGDA?

MAGDA is an AI-driven automation system that converts natural language prompts into domain-specific commands. Originally designed for Digital Audio Workstation (DAW) automation, MAGDA has evolved into a flexible, domain-agnostic platform that can be extended to any domain.

## Key Features

### 🎯 **Domain Agnostic**
- Extensible architecture supporting multiple domains
- Clean separation between core logic and domain-specific implementations
- Easy to add new domains (image editing, code generation, web automation, etc.)

### 🧠 **Intelligent Processing**
- Advanced semantic complexity detection
- Automatic model selection based on task complexity
- Multi-language support (English, Japanese, Spanish, French, German)

### ⚡ **High Performance**
- Fast-path routing for simple operations
- Intelligent caching of semantic models
- Optimized for speed and cost efficiency

### 🔧 **Developer Friendly**
- Clean, well-documented APIs
- Comprehensive test coverage
- Extensive documentation and examples

## Architecture Overview

MAGDA uses a sophisticated multi-agent architecture:

```
Natural Language Prompt
         ↓
   Complexity Detection
         ↓
    Model Selection
         ↓
   Operation Parsing
         ↓
   Agent Execution
         ↓
   Domain Commands
```

### Core Components

- **Pipeline**: Main entry point for processing prompts
- **Factory**: Creates domain-specific components
- **Orchestrator**: Coordinates complex operations
- **Agents**: Specialized components for different operation types
- **Complexity Detection**: Intelligent task analysis and model selection

## Supported Domains

### 🎵 **DAW (Digital Audio Workstation)**
- Track creation and management
- Volume and pan control
- Effect processing (reverb, compression, EQ)
- MIDI operations (notes, chords, quantization)
- Clip management

### 🖼️ **Image Editing** (Planned)
- Filter applications
- Transform operations
- Effect processing

### 💻 **Code Generation** (Planned)
- Function generation
- Class creation
- Test generation

### 🌐 **Web Automation** (Planned)
- Navigation
- Interaction
- Data scraping

## Multi-Language Support

MAGDA supports natural language prompts in multiple languages:

- **English**: "Create a new track called 'Bass'"
- **Japanese**: "トラック「ベース」の音量を-6dBに設定してください"
- **Spanish**: "Crear una nueva pista llamada 'Guitarra'"
- **French**: "Ajouter de la réverbération à la piste de batterie"
- **German**: "Erstellen Sie eine neue Spur namens 'Bass'"

## Performance Features

### Intelligent Model Selection

MAGDA automatically selects the best AI model based on task complexity:

- **Simple tasks** → `gpt-4o-mini` (fast, cost-effective)
- **Medium complexity** → `gpt-4o` (balanced performance)
- **Complex reasoning** → `gpt-4o-2024-07-18` (best performance)

### Fast-Path Routing

Simple operations bypass AI processing for maximum speed:

```python
# These use fast-path routing (no AI needed)
"create track bass"
"set volume -6db"
"add reverb"
"mute drums"
```

### Semantic Complexity Detection

Advanced analysis using sentence transformers for accurate complexity assessment:

```python
from magda import get_complexity_detector

detector = get_complexity_detector()
result = detector.detect_complexity("Add reverb to the drums track")
print(f"Complexity: {result.complexity} (confidence: {result.confidence})")
# Output: Complexity: simple (confidence: 0.85)
```

## Getting Started

### Quick Start

```python
from magda import MAGDAPipeline

# Initialize the pipeline
pipeline = MAGDAPipeline()

# Process a natural language prompt
result = pipeline.process_prompt("Create a new track called 'Bass'")

# Get the generated commands
print(result['daw_commands'])
# Output: ['track(name:Bass, id:uuid-1234-5678)']
```

### Installation

```bash
# Install from source
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python
pip install -e .

# Set up your API key
echo "OPENAI_API_KEY=your_key_here" > .env
```

### CLI Usage

```bash
# Basic usage
magda "Create a new track called 'Bass'"

# Multiple commands
magda "Create bass track, add compression, set volume -6dB"
```

## Use Cases

### 🎵 **Music Production**
- Quick track setup and organization
- Automated effect chain creation
- MIDI pattern generation
- Volume automation

### 🎬 **Audio Post-Production**
- Automated mixing workflows
- Effect processing chains
- Track organization
- Export preparation

### 🎤 **Podcast Production**
- Automated editing workflows
- Noise reduction setup
- Level normalization
- Export formatting

### 🎮 **Game Audio**
- Automated sound effect processing
- Music track management
- Dynamic mixing setups
- Asset organization

## Technology Stack

### Core Technologies
- **Python 3.12+**: Main development language
- **OpenAI API**: AI model integration
- **Sentence Transformers**: Semantic analysis
- **Pydantic**: Data validation and serialization

### Development Tools
- **pytest**: Testing framework
- **ruff**: Code formatting and linting
- **mypy**: Type checking
- **MkDocs**: Documentation generation

### Future Technologies
- **MCP (Model Context Protocol)**: Real-time DAW integration
- **C++**: Performance-critical components
- **WebAssembly**: Browser-based deployment

## Project Status

### Current Version: 1.1.0

**✅ Completed Features:**
- Core domain-agnostic architecture
- DAW domain implementation
- Multi-language support
- Intelligent model selection
- Fast-path routing
- Comprehensive testing
- Documentation system

**🚧 In Development:**
- MCP integration design
- Additional domain implementations
- Performance optimizations
- Extended CLI features

**📋 Planned Features:**
- Real-time DAW integration
- C++ backend components
- Web-based interface
- Plugin system
- Cloud deployment

## Contributing

We welcome contributions! Please see our [Contributing Guide](development/contributing.md) for details.

### Ways to Contribute
- **Code**: Implement new features or fix bugs
- **Documentation**: Improve guides and examples
- **Testing**: Add tests or improve test coverage
- **Feedback**: Report issues or suggest features
- **Domains**: Implement new domain support

### Development Setup

```bash
# Clone and setup
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python

# Install dependencies
pip install -e '.[dev,docs]'

# Run tests
pytest tests/

# Build documentation
mkdocs serve
```

## License

MAGDA is licensed under the MIT License. See the [LICENSE](../LICENSE) file for details.

## Acknowledgments

- **OpenAI**: For providing the AI models that power MAGDA
- **Sentence Transformers**: For semantic analysis capabilities
- **The Python Community**: For the excellent tools and libraries
- **Contributors**: Everyone who has helped improve MAGDA

## Contact

- **GitHub**: [lucaromagnoli/magda](https://github.com/lucaromagnoli/magda)
- **Issues**: [GitHub Issues](https://github.com/lucaromagnoli/magda/issues)
- **Discussions**: [GitHub Discussions](https://github.com/lucaromagnoli/magda/discussions)

---

**MAGDA** - Making automation accessible through natural language. 🚀
