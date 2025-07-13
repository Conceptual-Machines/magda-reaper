# MAGDA

[![CI/CD Pipeline](https://github.com/lucaromagnoli/magda/workflows/MAGDA%20CI%2FCD%20Pipeline/badge.svg)](https://github.com/lucaromagnoli/magda/actions)
[![Python 3.13](https://img.shields.io/badge/python-3.13-blue.svg)](https://www.python.org/downloads/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Code style: ruff](https://img.shields.io/badge/code%20style-ruff-000000.svg)](https://github.com/astral-sh/ruff)
[![Type checked with mypy](https://img.shields.io/badge/mypy-checked-blue.svg)](http://mypy-lang.org/)
[![Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen.svg)](https://github.com/lucaromagnoli/magda)

Multi Agent Generative DAW API

MAGDA is an AI-driven system that translates natural language prompts into generic DAW (Digital Audio Workstation) commands using a multi-agent architecture powered by OpenAI's Responses API.

## 🎵 Features

[![OpenAI](https://img.shields.io/badge/OpenAI-GPT--4.1-green.svg)](https://openai.com/)
[![Pydantic](https://img.shields.io/badge/Pydantic-2.11+-blue.svg)](https://pydantic.dev/)
[![Multi-Agent](https://img.shields.io/badge/Architecture-Multi--Agent-orange.svg)](https://en.wikipedia.org/wiki/Multi-agent_system)

- **Natural Language Processing**: Convert plain English (and other languages) into precise DAW commands
- **Multi-Agent Architecture**: Specialized agents for different DAW operations
- **Structured Output**: Validated JSON responses with Pydantic models
- **Multilingual Support**: Works with English, Spanish, French, German, Italian, and more
- **Operation Chaining**: Handle complex multi-step operations in a single prompt
- **Real-time Processing**: Fast response times with OpenAI's latest models

## 🏗️ Architecture

MAGDA uses a dual-stage pipeline:

1. **Operation Identifier Agent** (`o3-mini`): Analyzes prompts and identifies operations
2. **Specialized Agents** (`gpt-4.1`): Handle specific DAW operations:
   - **Track Agent**: Create, modify, and manage tracks
   - **Clip Agent**: Handle audio/MIDI clips and regions
   - **Volume Agent**: Control track and clip volumes
   - **Effect Agent**: Apply and configure audio effects
   - **MIDI Agent**: Manage MIDI data and events

## 🚀 Quick Start

[![uv](https://img.shields.io/badge/uv-Package%20Manager-purple.svg)](https://docs.astral.sh/uv/)
[![Python 3.12+](https://img.shields.io/badge/Python-3.12+-blue.svg)](https://www.python.org/downloads/)

### Prerequisites

- Python 3.12+
- OpenAI API key
- [uv](https://docs.astral.sh/uv/) package manager

### Installation

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd magda
   ```

2. **Install dependencies**:
   ```bash
   uv sync
   ```

3. **Set up environment**:
   ```bash
   cp env.example .env
   # Edit .env and add your OpenAI API key
   ```

4. **Install the package**:
   ```bash
   uv pip install -e .
   ```

### Usage

#### Command Line Interface

```bash
# Basic usage
magda "create a track for bass and one for drums"

# Complex multi-step operations
magda "create a bass track with Serum, add a compressor effect with 4:1 ratio, and set the volume to -6dB"

# Multilingual support
magda "crear una pista de bajo con Serum y aplicar compresión"
```

#### Python API

```python
from magda.pipeline import MAGDAPipeline

pipeline = MAGDAPipeline()
result = pipeline.process_prompt("create a track for bass with Serum")
print(result["daw_commands"])
```

## 📝 Examples

### Track Operations
```bash
# Create tracks
magda "create a track for bass and one for drums"
# Output: track(bass, serum), track(drums, addictive_drums)

# Modify tracks
magda "rename the bass track to 'Deep Bass' and change its color to blue"
```

### Effect Operations
```bash
# Add effects
magda "add a compressor to the bass track with 4:1 ratio and -20dB threshold"
# Output: effect(bass, compressor, ratio=4, threshold=-20)

# Complex effect chains
magda "add reverb with 0.3 wet mix and 2.5s decay, then add delay with 0.25 feedback"
```

### Volume and Mixing
```bash
# Volume control
magda "set the bass track volume to -6dB and the drums to -3dB"
# Output: volume(bass, -6), volume(drums, -3)

# Fade operations
magda "create a 2-second fade-in on the vocal track"
```

### MIDI Operations
```bash
# MIDI editing
magda "quantize the piano MIDI to 16th notes and transpose up by 2 semitones"
# Output: midi(piano, quantize=16th, transpose=2)
```

## 🔧 Configuration

### Environment Variables

Create a `.env` file with:

```env
OPENAI_API_KEY=your_openai_api_key_here
```

### Model Selection

MAGDA uses different models for different tasks:
- **Operation Identifier**: `o3-mini` (reasoning and analysis)
- **Specialized Agents**: `gpt-4.1` (structured output generation)

## 🧪 Testing

Run the test suite:

```bash
uv run pytest
```

Run with coverage:

```bash
uv run pytest --cov=magda
```

## 🏗️ Development

### Project Structure

```
magda/
├── magda/
│   ├── __init__.py
│   ├── main.py              # CLI entry point
│   ├── models.py            # Pydantic data models
│   ├── pipeline.py          # Main orchestration pipeline
│   └── agents/
│       ├── __init__.py
│       ├── base.py          # Base agent class
│       ├── operation_identifier.py
│       ├── track_agent.py
│       ├── clip_agent.py
│       ├── volume_agent.py
│       ├── effect_agent.py
│       └── midi_agent.py
├── scripts/
│   └── release.py           # Release automation script
├── pyproject.toml
├── uv.lock
└── README.md
```

### Release Process

MAGDA uses semantic versioning and automated releases. To create a new release:

1. **Bump version and release**:
   ```bash
   python scripts/release.py patch   # or 'minor' or 'major'
   ```

2. **Dry run** (see what would happen):
   ```bash
   python scripts/release.py patch --dry-run
   ```

3. **Prepare without pushing**:
   ```bash
   python scripts/release.py patch --no-push
   ```

The release script will:
- Bump the version in `pyproject.toml`
- Create a git tag
- Push changes and tag to GitHub
- Trigger the GitHub Actions workflow to create a release

### Adding New Agents

1. Create a new agent class inheriting from `BaseAgent`
2. Implement the `process` method with structured output
3. Add the agent to the pipeline's agent registry
4. Update the operation identifier to recognize new operations

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Acknowledgments

- OpenAI for the Responses API and GPT models
- The DAW community for inspiration and feedback
- Contributors and early adopters

## 📞 Support

For questions, issues, or contributions:
- Open an issue on GitHub
- Join our community discussions
- Check the documentation for common solutions
