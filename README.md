[![CI/CD Pipeline](https://github.com/lucaromagnoli/magda/workflows/MAGDA%20CI%2FCD%20Pipeline/badge.svg)](https://github.com/lucaromagnoli/magda/actions)
[![Python 3.11+](https://img.shields.io/badge/Python-3.11+-blue.svg)](https://www.python.org/downloads/)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-green.svg)](https://opensource.org/licenses/GPL-3.0)
[![codecov](https://codecov.io/gh/lucaromagnoli/magda/branch/main/graph/badge.svg)](https://codecov.io/gh/lucaromagnoli/magda)

# MAGDA

<div align="center">

```
     MAGDA
   🎵 🎹 🎤 🐍 ⚡ 🦀

  Multi Agent
 Generic DAW API
```

</div>

Multi Agent Generic DAW API

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

MAGDA is implemented in both **Python** and **C++** to provide maximum flexibility and performance:

### Python Implementation
- **High-level orchestration** and AI integration
- **Multi-agent pipeline** with OpenAI's Responses API
- **Rapid prototyping** and development
- **Rich ecosystem** of AI/ML libraries

### C++ Implementation
- **High-performance** core components
- **Low-latency** audio processing
- **Cross-platform** compatibility
- **Production-ready** DAW integration

### Dual-Stage Pipeline

1. **Orchestrator Agent** (`gpt-4.1-nano`): Analyzes prompts and identifies operations
2. **Specialized Agents** (`gpt-4o-mini`): Handle specific DAW operations:
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

#### Python Implementation

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd magda
   ```

2. **Install Python dependencies**:
   ```bash
   cd python
   uv sync
   ```

3. **Set up environment**:
   ```bash
   cp env.example .env
   # Edit .env and add your OpenAI API key
   ```

4. **Install the Python package**:
   ```bash
   uv pip install -e .
   ```

#### C++ Implementation

1. **Build C++ components**:
   ```bash
   cd cpp
   mkdir build && cd build
   cmake ..
   make
   ```

2. **Run C++ examples**:
   ```bash
   ./examples/magda_example
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
- **Orchestrator Agent**: `gpt-4.1-nano` (fast, accurate operation identification)
- **Specialized Agents**: `gpt-4o-mini` (ultra-fast, cost-effective DAW command generation)

## 🧪 Testing

### Python Tests

Run the Python test suite:

```bash
cd python
uv run pytest
```

Run with coverage:

```bash
uv run pytest --cov=magda
```

### C++ Tests

Run the C++ test suite:

```bash
cd cpp
mkdir build && cd build
cmake ..
make
ctest
```

## 🏗️ Development

### Project Structure

```
magda/
├── python/                  # Python implementation
│   ├── magda/
│   │   ├── __init__.py
│   │   ├── main.py          # CLI entry point
│   │   ├── models.py        # Pydantic data models
│   │   ├── pipeline.py      # Main orchestration pipeline
│   │   ├── context_manager.py # Context management
│   │   └── agents/
│   │       ├── __init__.py
│   │       ├── base.py      # Base agent class
│   │       ├── orchestrator_agent.py
│   │       ├── track_agent.py
│   │       ├── clip_agent.py
│   │       ├── volume_agent.py
│   │       ├── effect_agent.py
│   │       └── midi_agent.py
│   ├── tests/               # Python test suite
│   ├── pyproject.toml       # Python package config
│   └── uv.lock
├── cpp/                     # C++ implementation
│   ├── include/magda_cpp/   # C++ headers
│   ├── src/                 # C++ source files
│   ├── examples/            # C++ examples
│   ├── tests/               # C++ test suite
│   └── CMakeLists.txt       # CMake configuration
├── scripts/
│   └── release.py           # Release automation script
├── .github/workflows/       # CI/CD pipelines
├── pyproject.toml           # Root project config
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
4. Update the orchestrator agent to recognize new operations

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## 📄 License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.



## 📞 Support

For questions, issues, or contributions:
- Open an issue on GitHub
- Join our community discussions
- Check the documentation for common solutions

## 🤖 Model Selection & Defaults

MAGDA uses multiple OpenAI models for different stages of the pipeline. The defaults are:

- **Orchestrator Agent**: `gpt-4.1-nano` (fast, accurate operation identification)
- **Specialized Agents**: `gpt-4o-mini` (ultra-fast, cost-effective for DAW commands)

You can override the model for any agent or operation by passing a `model` parameter to the pipeline or agent call. For complex reasoning tasks, you may use O-series models (e.g., `o3-mini`, `o1-pro`) which excel at multi-step reasoning but are slower and more expensive.

### Model Summary Table
| Model           | Latency | Token Usage | Cost | Reasoning | Recommended For         |
|-----------------|---------|-------------|------|-----------|------------------------|
| gpt-4o-mini     | ⭐⭐⭐    | ⭐⭐⭐        | ⭐⭐⭐ | ⭐         | All DAW tasks          |
| gpt-4.1-nano    | ⭐⭐⭐    | ⭐⭐⭐        | ⭐⭐⭐ | ⭐         | All DAW tasks          |
| gpt-4o          | ⭐⭐     | ⭐⭐         | ⭐⭐  | ⭐⭐        | Fallback, complex tasks|
| o3-mini         | ⭐      | ⭐          | ⭐   | ⭐⭐⭐       | Complex reasoning tasks|
| o1-pro          | ⭐      | ⭐          | ⭐   | ⭐⭐⭐       | Complex reasoning tasks|

## 📊 Model Benchmarking

MAGDA includes a benchmarking framework to evaluate model performance (latency, token usage, cost, etc.) for DAW tasks.

- **Run the benchmark:**
  ```bash
  python python/run_model_benchmark.py --config quick
  ```
  (Other configs: `full`, `latency`, `multistep`, or custom YAML)

- **Analyze results:**
  ```bash
  python python/analyze_benchmark_results.py
  ```
  This prints per-model and overall stats for latency, token usage, and more.

- **Customize:**
  Edit `python/run_model_benchmark.py` or provide your own config to test different prompts, models, or metrics.

---

For more details, see the `python/run_model_benchmark.py` and `python/analyze_benchmark_results.py` scripts for benchmarking methodology and results.
