# MAGDA Python Implementation

This directory contains the Python implementation of MAGDA (Multi Agent Domain Automation), featuring a domain-agnostic architecture that can work with any domain (DAW, Desktop, Web, etc.).

## 📁 Directory Structure

```
python/
├── README.md                    # This file
├── pyproject.toml              # Python package configuration
├── uv.lock                     # Dependency lock file
├── env.example                 # Environment variables template
├── pytest.ini                 # Pytest configuration
├── magda/                      # Main package
│   ├── __init__.py
│   ├── core/                   # Domain-agnostic core
│   │   ├── domain.py          # Abstract interfaces
│   │   └── pipeline.py        # Main pipeline
│   ├── domains/                # Domain implementations
│   │   └── daw/               # DAW domain (implemented)
│   │       ├── __init__.py
│   │       ├── daw_agents.py  # DAW-specific agents
│   │       └── daw_factory.py # DAW factory
│   ├── agents/                 # Legacy agents (to be migrated)
│   ├── config.py              # Configuration
│   ├── models.py              # Data models
│   └── utils.py               # Utilities
├── examples/                   # Usage examples
│   ├── README.md
│   └── example_domain_agnostic.py
├── benchmarks/                 # Performance benchmarks
│   ├── README.md
│   ├── run_model_benchmark.py
│   ├── analyze_benchmark.py
│   └── *.json                 # Benchmark results
├── scripts/                    # Utility scripts
│   ├── README.md
│   ├── check_*.py             # Model checking scripts
│   └── debug_*.py             # Debugging scripts
├── tests/                      # Test suite
│   ├── test_*.py              # Unit and integration tests
│   └── conftest.py            # Pytest configuration
└── docs/                       # Documentation (future)
    └── README.md
```

## 🚀 Quick Start

### Prerequisites
- Python 3.12+
- [uv](https://docs.astral.sh/uv/) package manager
- OpenAI API key

### Installation

1. **Install dependencies**:
   ```bash
   uv pip install -e '.[dev,docs]'
   ```

2. **Set up environment**:
   ```bash
   cp env.example .env
   # Edit .env and add your OpenAI API key
   ```

3. **Run tests**:
   ```bash
   pytest
   ```

## 🏗️ Architecture

### Domain-Agnostic Core
- **`magda/core/domain.py`**: Abstract interfaces for domains, agents, and pipelines
- **`magda/core/pipeline.py`**: Main domain-agnostic pipeline

### Domain Implementations
- **`magda/domains/daw/`**: DAW domain implementation (currently implemented)
- **Future domains**: Desktop, Web, Mobile, Cloud, Business

### Key Components
- **DomainAgent**: Abstract base for all agents
- **DomainOrchestrator**: Abstract base for orchestrators
- **DomainPipeline**: Abstract base for pipelines
- **DomainFactory**: Factory pattern for domain creation

## 📚 Examples

### Running Examples
```bash
# Domain-agnostic demo
python examples/example_domain_agnostic.py
```

### Example Usage
```python
from magda.core.domain import DomainType
from magda.core.pipeline import MAGDACorePipeline
from magda.domains.daw import DAWFactory

# Create DAW pipeline
daw_factory = DAWFactory()
pipeline = MAGDACorePipeline(daw_factory, DomainType.DAW)

# Set host context
pipeline.set_host_context({
    "vst_plugins": ["serum", "addictive drums"],
    "track_names": ["bass", "drums", "guitar"]
})

# Process prompts
result = pipeline.process_prompt("create bass track with serum")
```

## 🧪 Testing

### Running Tests
```bash
# Run all tests
pytest

# Run specific test categories
pytest tests/test_*.py
pytest benchmarks/test_*.py

# Run with coverage
pytest --cov=magda --cov-report=html
```

### Test Categories
- **Unit Tests**: `tests/test_*.py`
- **Integration Tests**: `tests/test_integration.py`
- **Benchmark Tests**: `benchmarks/test_*.py`

## 📊 Benchmarks

### Running Benchmarks
```bash
# Model performance benchmark
python benchmarks/run_model_benchmark.py

# Operations benchmark
python benchmarks/test_operations_benchmark.py

# Analyze results
python benchmarks/analyze_benchmark.py benchmarks/results.json
```

### Benchmark Categories
- **Model Performance**: Tests different OpenAI models
- **Complexity Detection**: Compares algorithmic vs semantic detection
- **Operations**: Tests actual DAW operations

## 🛠️ Development

### Adding New Domains
1. Create domain directory: `magda/domains/your_domain/`
2. Implement agents inheriting from `DomainAgent`
3. Create factory implementing `DomainFactory`
4. Add tests and documentation

### Code Quality
```bash
# Linting
ruff check .

# Type checking
mypy magda/

# Security checks
bandit -r magda/
```

### Documentation
```bash
# Install docs dependencies
uv pip install '.[docs]'

# Build documentation (when mkdocs is set up)
mkdocs build
```

## 🔧 Scripts

### Utility Scripts
```bash
# Check available models
python scripts/check_available_models.py

# Debug specific functionality
python scripts/debug_clip.py

# Capture sample data
python scripts/capture_samples.py
```

## 📦 Package Management

### Dependencies
- **Core**: `openai`, `pydantic`, `python-dotenv`
- **Dev**: `pytest`, `ruff`, `mypy`, `bandit`
- **Docs**: `mkdocs`, `mkdocs-material`, `mkdocstrings`

### Installation Commands
```bash
# Core package
uv pip install -e .

# With development dependencies
uv pip install -e '.[dev]'

# With documentation dependencies
uv pip install -e '.[docs]'

# With all dependencies
uv pip install -e '.[dev,docs]'
```

## 🤝 Contributing

### Development Workflow
1. **Fork the repository**
2. **Create a feature branch**
3. **Make your changes**
4. **Add tests**
5. **Update documentation**
6. **Submit a pull request**

### Code Standards
- Use type hints throughout
- Follow Google-style docstrings
- Include examples in docstrings
- Add tests for new features
- Update relevant documentation

### Testing Guidelines
- Write unit tests for all new code
- Include integration tests for complex features
- Add benchmark tests for performance-critical code
- Ensure all tests pass before submitting PR

## 📄 License

This project is licensed under the GPL-3.0 License - see the [LICENSE](../LICENSE) file for details.

---

For more information, see:
- [Main README](../README.md) - Project overview
- [Examples](examples/README.md) - Usage examples
- [Benchmarks](benchmarks/README.md) - Performance testing
- [Scripts](scripts/README.md) - Utility scripts
