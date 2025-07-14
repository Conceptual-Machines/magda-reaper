# MAGDA Examples

This directory contains example scripts demonstrating how to use MAGDA's domain-agnostic architecture.

## Available Examples

### Domain-Agnostic Demo
- **File**: `example_domain_agnostic.py`
- **Description**: Demonstrates the domain-agnostic architecture with DAW domain
- **Usage**: `python examples/example_domain_agnostic.py`

## Running Examples

### Prerequisites
1. Install MAGDA with development dependencies:
   ```bash
   uv pip install -e '.[dev,docs]'
   ```

2. Set up your environment:
   ```bash
   cp env.example .env
   # Edit .env and add your OpenAI API key
   ```

### Running the Domain-Agnostic Demo
```bash
python examples/example_domain_agnostic.py
```

This example demonstrates:
- Creating domain-specific pipelines
- Setting host context (VST plugins, track names)
- Processing prompts through different domains
- Domain switching capabilities

## Adding New Examples

When adding new examples:

1. **Create the example file** in this directory
2. **Add a description** to this README
3. **Include proper docstrings** and type hints
4. **Test the example** to ensure it works
5. **Update this README** with usage instructions

## Example Structure

```python
#!/usr/bin/env python3
"""
Example: [Brief description]

This example demonstrates [specific functionality].
"""

import os
from dotenv import load_dotenv
from magda.core.domain import DomainType
from magda.core.pipeline import MAGDACorePipeline

def main():
    """Main example function."""
    # Your example code here
    pass

if __name__ == "__main__":
    main()
```
