# MAGDA Documentation

This directory contains the comprehensive documentation for MAGDA (Multi Agent Domain Automation).

## 📚 Documentation Structure

```
docs/
├── README.md                    # This file - documentation overview
├── api/                         # API documentation
│   ├── core/                    # Core domain-agnostic APIs
│   │   ├── domain.md           # Domain interfaces and types
│   │   ├── pipeline.md         # Core pipeline documentation
│   │   └── agents.md           # Base agent documentation
│   ├── domains/                 # Domain-specific documentation
│   │   ├── daw/                # DAW domain documentation
│   │   ├── desktop/            # Desktop domain documentation (future)
│   │   └── web/                # Web domain documentation (future)
│   └── examples/                # Code examples and tutorials
├── guides/                      # User guides and tutorials
│   ├── getting-started.md      # Quick start guide
│   ├── domain-creation.md      # How to create new domains
│   ├── host-integration.md     # Host integration guide
│   └── best-practices.md       # Best practices and patterns
├── architecture/                # Architecture documentation
│   ├── overview.md             # High-level architecture
│   ├── domain-agnostic.md      # Domain-agnostic design
│   ├── pipeline-flow.md        # Pipeline flow diagrams
│   └── decision-records/       # Architecture Decision Records (ADRs)
├── development/                 # Development documentation
│   ├── contributing.md         # Contribution guidelines
│   ├── testing.md              # Testing guide
│   ├── deployment.md           # Deployment guide
│   └── release-process.md      # Release process
└── assets/                      # Documentation assets
    ├── images/                  # Diagrams and screenshots
    ├── diagrams/                # Mermaid diagrams source
    └── examples/                # Example files and data
```

## 🚀 Quick Navigation

### For Users
- **[Getting Started](guides/getting-started.md)** - Quick start guide
- **[API Reference](api/core/domain.md)** - Core API documentation
- **[Domain Creation](guides/domain-creation.md)** - How to add new domains
- **[Host Integration](guides/host-integration.md)** - Integrating with hosts

### For Developers
- **[Architecture Overview](architecture/overview.md)** - System architecture
- **[Contributing](development/contributing.md)** - How to contribute
- **[Testing](development/testing.md)** - Testing guidelines
- **[Release Process](development/release-process.md)** - Release workflow

### For Domain Implementers
- **[Domain-Agnostic Design](architecture/domain-agnostic.md)** - Core design principles
- **[DAW Domain](api/domains/daw/)** - DAW domain implementation
- **[Domain Creation Guide](guides/domain-creation.md)** - Step-by-step guide

## 🔧 Documentation Automation

### Automated Generation

The documentation is automatically generated and updated using:

1. **API Documentation**: Generated from docstrings using [MkDocs](https://www.mkdocs.org/)
2. **Code Examples**: Extracted from test files and examples
3. **Architecture Diagrams**: Generated from Mermaid source files
4. **Domain Documentation**: Auto-generated from domain implementations

### Build Commands

```bash
# Install documentation dependencies
pip install mkdocs mkdocs-material mkdocstrings[python] mkdocs-autorefs

# Build documentation
mkdocs build

# Serve documentation locally
mkdocs serve

# Deploy to GitHub Pages
mkdocs gh-deploy
```

### Configuration

The documentation is configured in `mkdocs.yml` at the root of the project:

```yaml
site_name: MAGDA Documentation
site_description: Multi Agent Domain Automation
site_author: MAGDA Team
repo_url: https://github.com/lucaromagnoli/magda

theme:
  name: material
  features:
    - navigation.tabs
    - navigation.sections
    - navigation.expand
    - search.suggest
    - search.highlight

plugins:
  - search
  - autorefs
  - mkdocstrings:
      default_handler: python
      handlers:
        python:
          paths: [../python]
          options:
            show_source: true
            show_root_heading: true
```

## 📖 Documentation Standards

### Code Documentation

All code should follow these documentation standards:

1. **Docstrings**: Use Google-style docstrings for all public APIs
2. **Type Hints**: Include type hints for all function parameters and return values
3. **Examples**: Include usage examples in docstrings
4. **Cross-references**: Link to related classes and methods

### Example Docstring

```python
def process_prompt(self, prompt: str) -> Dict[str, Any]:
    """
    Process a natural language prompt through the domain pipeline.

    This method takes a natural language prompt and processes it through
    the appropriate domain agents to generate domain-specific commands.

    Args:
        prompt: Natural language prompt to process (e.g., "create bass track with serum")

    Returns:
        Dictionary containing:
            - success: Boolean indicating if processing was successful
            - commands: List of domain-specific commands
            - domain_type: Type of domain used
            - domain_context: Context information from host

    Raises:
        RuntimeError: If domain pipeline is not initialized
        TimeoutError: If processing takes too long

    Example:
        >>> pipeline = MAGDACorePipeline(daw_factory, DomainType.DAW)
        >>> result = pipeline.process_prompt("create bass track with serum")
        >>> print(result["commands"])
        ['track(name:bass,vst:serum,type:midi)']
    """
```

### Architecture Documentation

1. **Decision Records**: Use ADRs for significant architectural decisions
2. **Diagrams**: Use Mermaid for all diagrams
3. **Examples**: Include practical examples for all concepts
4. **Cross-references**: Link between related documentation

## 🔄 Documentation Workflow

### For New Features

1. **Update Code**: Add proper docstrings and type hints
2. **Update Guides**: Add relevant user guides
3. **Update API Docs**: Ensure API documentation is current
4. **Add Examples**: Include practical examples
5. **Update Diagrams**: Update architecture diagrams if needed

### For New Domains

1. **Domain Documentation**: Create domain-specific documentation
2. **API Reference**: Document domain agents and factories
3. **Examples**: Add domain-specific examples
4. **Integration Guide**: Document host integration patterns
5. **Testing Guide**: Document testing strategies

### Automated Checks

The CI/CD pipeline includes documentation checks:

- **Docstring Coverage**: Ensure all public APIs are documented
- **Link Validation**: Check that all internal links work
- **Example Validation**: Verify that code examples are valid
- **Build Validation**: Ensure documentation builds successfully

## 📊 Documentation Metrics

We track documentation quality using:

- **Docstring Coverage**: Percentage of public APIs with docstrings
- **Example Coverage**: Percentage of features with examples
- **Link Health**: Percentage of working internal links
- **Build Success**: Documentation build success rate

## 🎯 Documentation Goals

### Short-term (Next Release)
- [ ] Complete core API documentation
- [ ] Add comprehensive examples
- [ ] Create domain creation guide
- [ ] Set up automated documentation builds

### Medium-term (Next Quarter)
- [ ] Add all domain documentation
- [ ] Create video tutorials
- [ ] Add interactive examples
- [ ] Implement documentation search

### Long-term (Next Year)
- [ ] Multi-language documentation
- [ ] Interactive API explorer
- [ ] Community documentation
- [ ] Documentation analytics

## 🤝 Contributing to Documentation

### Guidelines

1. **Write for Users**: Focus on what users need to know
2. **Include Examples**: Every concept should have practical examples
3. **Keep Current**: Documentation should match the code
4. **Be Consistent**: Follow established patterns and styles
5. **Test Examples**: Ensure all code examples work

### How to Contribute

1. **Fork the Repository**: Create your own fork
2. **Create a Branch**: Work on a feature branch
3. **Update Documentation**: Make your changes
4. **Test Locally**: Build and test documentation locally
5. **Submit PR**: Create a pull request with your changes

### Documentation Review Process

1. **Automated Checks**: CI/CD runs documentation checks
2. **Peer Review**: At least one other person reviews changes
3. **User Testing**: Test with actual users when possible
4. **Continuous Improvement**: Regular documentation audits

---

For questions about documentation, please:
- Open an issue on GitHub
- Check the [contributing guide](development/contributing.md)
- Join our community discussions
