# Contributing to MAGDA

Thank you for your interest in contributing to MAGDA! This guide will help you get started.

## Getting Started

### Prerequisites

- Python 3.12 or higher
- [uv](https://docs.astral.sh/uv/) package manager (recommended)
- OpenAI API key for testing
- Git

### Development Setup

1. **Fork and clone the repository**
   ```bash
   git clone https://github.com/your-username/magda.git
   cd magda/python
   ```

2. **Install dependencies**
   ```bash
   uv sync
   uv pip install -e '.[dev,docs]'
   ```

3. **Set up environment**
   ```bash
   cp env.example .env
   # Edit .env and add your OpenAI API key
   ```

4. **Run tests**
   ```bash
   pytest tests/
   ```

## Development Workflow

### 1. Create a Feature Branch

```bash
git checkout -b feature/amazing-feature
```

### 2. Make Your Changes

- Write your code
- Add tests for new functionality
- Update documentation
- Follow the coding standards below

### 3. Test Your Changes

```bash
# Run all tests
pytest tests/

# Run specific test categories
pytest tests/test_*.py
pytest benchmarks/test_*.py

# Run with coverage
pytest --cov=magda --cov-report=html
```

### 4. Check Code Quality

```bash
# Linting
ruff check .

# Type checking
mypy magda/

# Security checks
bandit -r magda/
```

### 5. Commit Your Changes

```bash
git add .
git commit -m "feat: add amazing feature

- Add new functionality
- Update tests
- Update documentation"
```

### 6. Push and Create Pull Request

```bash
git push origin feature/amazing-feature
```

Then create a pull request on GitHub.

## Coding Standards

### Python Style Guide

- Follow [PEP 8](https://pep8.org/) style guidelines
- Use type hints throughout
- Write docstrings for all public functions and classes
- Use meaningful variable and function names

### Code Formatting

We use `ruff` for code formatting and linting:

```bash
# Format code
ruff format .

# Check for issues
ruff check .

# Auto-fix issues
ruff check --fix .
```

### Type Hints

Always use type hints for function parameters and return values:

```python
from typing import Dict, List, Optional

def process_prompt(prompt: str, context: Optional[Dict] = None) -> Dict[str, any]:
    """Process a natural language prompt.

    Args:
        prompt: The input prompt to process
        context: Optional context information

    Returns:
        Dictionary containing processing results
    """
    # Implementation
    pass
```

### Docstrings

Use Google-style docstrings:

```python
def create_track(name: str, track_type: str = "audio") -> TrackResult:
    """Create a new track in the DAW.

    Args:
        name: Name of the track to create
        track_type: Type of track ('audio', 'midi', 'instrument')

    Returns:
        TrackResult containing track information

    Raises:
        ValueError: If track name is invalid
        RuntimeError: If track creation fails
    """
    # Implementation
    pass
```

## Testing Guidelines

### Writing Tests

- Write unit tests for all new code
- Include integration tests for complex features
- Add benchmark tests for performance-critical code
- Ensure all tests pass before submitting PR

### Test Structure

```python
import pytest
from magda import MAGDAPipeline

class TestMAGDAPipeline:
    """Test cases for MAGDAPipeline."""

    def setup_method(self):
        """Set up test fixtures."""
        self.pipeline = MAGDAPipeline()

    def test_process_simple_prompt(self):
        """Test processing a simple prompt."""
        result = self.pipeline.process_prompt("Create a new track")
        assert result['success'] is True
        assert 'commands' in result

    def test_process_complex_prompt(self):
        """Test processing a complex prompt."""
        result = self.pipeline.process_prompt(
            "Create a bass track and add compression"
        )
        assert result['success'] is True
        assert len(result['daw_commands']) >= 2
```

### Running Tests

```bash
# Run all tests
pytest

# Run specific test file
pytest tests/test_pipeline.py

# Run tests with coverage
pytest --cov=magda --cov-report=html

# Run slow tests only
pytest -m slow

# Skip slow tests
pytest -m "not slow"
```

## Documentation

### Updating Documentation

- Update docstrings when changing public APIs
- Add examples for new features
- Update README files if needed
- Build and check documentation locally

### Building Documentation

```bash
# Install docs dependencies
uv pip install '.[docs]'

# Build documentation
mkdocs build

# Serve documentation locally
mkdocs serve
```

### Documentation Standards

- Use clear, concise language
- Include code examples
- Keep documentation up to date
- Use proper markdown formatting

## Adding New Features

### 1. Plan Your Feature

- Define the feature requirements
- Consider the impact on existing code
- Plan the API design
- Consider testing strategy

### 2. Implement the Feature

- Follow the coding standards
- Write comprehensive tests
- Update documentation
- Consider performance implications

### 3. Test Thoroughly

- Unit tests for individual components
- Integration tests for feature workflow
- Performance tests if applicable
- Manual testing with real scenarios

### 4. Update Documentation

- Update API documentation
- Add usage examples
- Update README if needed
- Add migration guide if breaking changes

## Adding New Domains

### 1. Create Domain Structure

```bash
mkdir -p magda/domains/your_domain
touch magda/domains/your_domain/__init__.py
touch magda/domains/your_domain/your_domain_agents.py
touch magda/domains/your_domain/your_domain_factory.py
```

### 2. Implement Domain Components

```python
# your_domain_factory.py
from magda.core.domain import DomainFactory, DomainType

class YourDomainFactory(DomainFactory):
    def create_orchestrator(self) -> DomainOrchestrator:
        return YourDomainOrchestrator()

    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "operation1": Operation1Agent(),
            "operation2": Operation2Agent(),
        }

    def create_pipeline(self) -> DomainPipeline:
        return DomainPipeline(self)

    def create_context(self) -> DomainContext:
        return DomainContext(
            domain_type=DomainType.YOUR_DOMAIN,
            domain_config={},
            host_context={},
            session_state={}
        )
```

### 3. Add Tests

```python
# tests/test_your_domain.py
import pytest
from magda.domains.your_domain import YourDomainFactory

def test_your_domain_factory():
    factory = YourDomainFactory()

    # Test component creation
    context = factory.create_context()
    assert context.domain_type == DomainType.YOUR_DOMAIN

    agents = factory.create_agents()
    assert "operation1" in agents
```

### 4. Update Documentation

- Add domain-specific documentation
- Include usage examples
- Update API reference
- Add to main README

## Bug Reports

### Reporting Bugs

When reporting bugs, please include:

- **Description**: Clear description of the bug
- **Steps to reproduce**: Step-by-step instructions
- **Expected behavior**: What should happen
- **Actual behavior**: What actually happens
- **Environment**: Python version, OS, dependencies
- **Error messages**: Full error traceback if applicable

### Example Bug Report

```
**Bug Description**
MAGDA fails to process Japanese prompts containing certain characters.

**Steps to Reproduce**
1. Initialize MAGDAPipeline
2. Call process_prompt("トラック「ベース」の音量を設定")
3. Observe error

**Expected Behavior**
Should process the Japanese prompt successfully.

**Actual Behavior**
Raises UnicodeDecodeError.

**Environment**
- Python 3.12.7
- macOS 14.0
- MAGDA v1.1.0

**Error Message**
UnicodeDecodeError: 'utf-8' codec can't decode byte 0x8f in position 15
```

## Feature Requests

### Suggesting Features

When suggesting features, please include:

- **Description**: Clear description of the feature
- **Use case**: Why this feature is needed
- **Proposed implementation**: How it could be implemented
- **Alternatives considered**: Other approaches you considered

### Example Feature Request

```
**Feature Description**
Add support for real-time DAW integration through MCP (Model Context Protocol).

**Use Case**
Currently MAGDA generates commands but doesn't execute them in real DAWs.
MCP integration would allow direct control of DAW software.

**Proposed Implementation**
- Create MCP client wrapper
- Implement DAW connection interfaces
- Add real-time state querying
- Support multiple DAW types (Ableton, Pro Tools, etc.)

**Alternatives Considered**
- OSC protocol (less standardized)
- MIDI SysEx (limited functionality)
- Custom API (more maintenance overhead)
```

## Code Review Process

### Review Checklist

Before submitting a PR, ensure:

- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] Documentation is updated
- [ ] No breaking changes (or migration guide provided)
- [ ] Performance impact is considered
- [ ] Security implications are addressed

### Review Guidelines

- Be constructive and respectful
- Focus on code quality and functionality
- Consider maintainability and readability
- Check for potential bugs or edge cases
- Verify test coverage is adequate

## Release Process

### Versioning

We follow [Semantic Versioning](https://semver.org/):

- **Major version**: Breaking changes
- **Minor version**: New features, backward compatible
- **Patch version**: Bug fixes, backward compatible

### Release Checklist

- [ ] All tests pass
- [ ] Documentation is up to date
- [ ] Version number is updated
- [ ] Changelog is updated
- [ ] Release notes are written
- [ ] Package is built and tested

## Getting Help

### Questions and Support

- **GitHub Issues**: For bugs and feature requests
- **GitHub Discussions**: For questions and general discussion
- **Documentation**: Check the docs first
- **Code Examples**: Look at the examples directory

### Community Guidelines

- Be respectful and inclusive
- Help others learn and contribute
- Share knowledge and best practices
- Follow the project's code of conduct

## License

By contributing to MAGDA, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to MAGDA! Your help makes this project better for everyone. 🚀
