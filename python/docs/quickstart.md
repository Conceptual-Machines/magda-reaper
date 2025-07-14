# Quick Start Guide

Get up and running with MAGDA in minutes!

## Installation

### Prerequisites
- Python 3.12 or higher
- OpenAI API key

### Install MAGDA

```bash
# Install from source (recommended for now)
git clone https://github.com/lucaromagnoli/magda.git
cd magda/python
pip install -e .
```

### Set up your API key

Create a `.env` file in the project root:

```bash
OPENAI_API_KEY=your_openai_api_key_here
```

## Basic Usage

### Simple Example

```python
from magda import MAGDAPipeline

# Initialize the pipeline
pipeline = MAGDAPipeline()

# Process a natural language prompt
result = pipeline.process_prompt("Create a new track called 'Bass'")

# Get the generated commands
print(result['commands'])
```

### More Examples

```python
# Volume control
result = pipeline.process_prompt("Set the volume of track 'Guitar' to -6dB")
print(f"Volume command: {result['daw_commands']}")

# Effect processing
result = pipeline.process_prompt("Add reverb to the drums track")
print(f"Effect command: {result['daw_commands']}")

# MIDI operations
result = pipeline.process_prompt("Create a 4-bar clip with a C major chord")
print(f"MIDI command: {result['daw_commands']}")

# Complex operations
result = pipeline.process_prompt("Mute all tracks except the vocals")
print(f"Complex commands: {result['daw_commands']}")
```

## Domain-Agnostic Usage

MAGDA supports multiple domains through its factory pattern:

```python
from magda import DomainPipeline, DAWFactory

# Create a DAW domain factory
daw_factory = DAWFactory()

# Create a domain-agnostic pipeline
pipeline = DomainPipeline(daw_factory)

# Process prompts
result = pipeline.process_prompt("Create a new track")
```

## CLI Usage

You can also use MAGDA from the command line:

```bash
magda "Create a new track called 'Bass'"
```

## Next Steps

- Check out the [API Reference](api/) for detailed documentation
- Explore [Examples](examples/) for more use cases
- Learn about the [Architecture](development/architecture.md) to understand how MAGDA works
