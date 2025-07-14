# MAGDA Scripts

This directory contains utility scripts for debugging, testing, and development of MAGDA.

## Available Scripts

### Model Checking Scripts
- **File**: `check_available_models.py`
- **Description**: Checks which OpenAI models are available and their capabilities
- **Usage**: `python scripts/check_available_models.py`

- **File**: `check_responses_models.py`
- **Description**: Tests OpenAI Responses API models specifically
- **Usage**: `python scripts/check_responses_models.py`

### Debugging Scripts
- **File**: `debug_clip.py`
- **Description**: Debug clip agent functionality
- **Usage**: `python scripts/debug_clip.py`

### Data Collection Scripts
- **File**: `capture_samples.py`
- **Description**: Captures sample data for testing and comparison
- **Usage**: `python scripts/capture_samples.py`

## Running Scripts

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

### Running Model Check Scripts
```bash
# Check available models
python scripts/check_available_models.py

# Check Responses API models
python scripts/check_responses_models.py
```

### Running Debug Scripts
```bash
# Debug clip functionality
python scripts/debug_clip.py
```

### Running Data Collection Scripts
```bash
# Capture sample data
python scripts/capture_samples.py
```

## Script Categories

### Model Testing
These scripts help verify that different OpenAI models are working correctly and have the expected capabilities.

### Debugging
These scripts help isolate and debug specific functionality in MAGDA agents and components.

### Data Collection
These scripts help gather sample data for testing, benchmarking, and development.

## Adding New Scripts

When adding new scripts:

1. **Create the script file** in this directory
2. **Add a description** to this README
3. **Include proper error handling** and logging
4. **Add docstrings** and type hints
5. **Test the script** to ensure it works
6. **Update this README** with usage instructions

## Script Structure

```python
#!/usr/bin/env python3
"""
Script: [Brief description]

This script [specific functionality].
"""

import os
import sys
from typing import Dict, Any
from dotenv import load_dotenv

def main():
    """Main script function."""
    # Load environment
    load_dotenv()

    # Check prerequisites
    if not os.getenv("OPENAI_API_KEY"):
        print("Error: OPENAI_API_KEY not set")
        sys.exit(1)

    # Your script code here
    pass

if __name__ == "__main__":
    main()
```

## Common Patterns

### Environment Setup
```python
import os
from dotenv import load_dotenv

load_dotenv()
api_key = os.getenv("OPENAI_API_KEY")
```

### Error Handling
```python
try:
    # Your code here
    pass
except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
```

### Logging
```python
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

logger.info("Starting script...")
```
