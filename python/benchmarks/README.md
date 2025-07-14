# MAGDA Benchmarks

This directory contains benchmark scripts and results for testing MAGDA's performance across different models, complexity detection methods, and operations.

## Available Benchmarks

### Model Performance Benchmarks
- **File**: `run_model_benchmark.py`
- **Description**: Tests different OpenAI models for latency, token usage, and accuracy
- **Results**: `model_performance_results.json`

### Complexity Detection Benchmarks
- **File**: `test_complexity_benchmark.py`
- **Description**: Compares algorithmic vs semantic complexity detection
- **Results**: Various JSON result files

### Operations Benchmarks
- **File**: `test_operations_benchmark.py`
- **Description**: Tests actual DAW operations with different complexity detection methods
- **Results**: `operations_benchmark_results_*.json`

### Analysis Scripts
- **File**: `analyze_benchmark.py`
- **Description**: Analyzes benchmark results and generates reports
- **File**: `analyze_benchmark_results.py`
- **Description**: Legacy analysis script
- **File**: `analyze_operations.py`
- **Description**: Analyzes operations benchmark results

## Running Benchmarks

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

### Running Model Performance Benchmark
```bash
python benchmarks/run_model_benchmark.py
```

### Running Complexity Detection Benchmark
```bash
python benchmarks/test_complexity_benchmark.py
```

### Running Operations Benchmark
```bash
python benchmarks/test_operations_benchmark.py
```

### Analyzing Results
```bash
python benchmarks/analyze_benchmark.py benchmarks/model_performance_results.json
```

## Benchmark Results

### Model Performance
- **Latency**: Response times for different models
- **Token Usage**: Number of tokens consumed
- **Cost**: Estimated API costs
- **Success Rate**: Percentage of successful operations

### Complexity Detection
- **Algorithmic**: Rule-based complexity detection
- **Semantic**: ML-based complexity detection
- **Accuracy**: Comparison of detection accuracy
- **Performance**: Speed and resource usage

### Operations
- **Success Rate**: Percentage of successful operations
- **Command Generation**: Quality of generated commands
- **Latency**: Response times for different operations
- **Model Usage**: Which models are used for different operations

## Adding New Benchmarks

When adding new benchmarks:

1. **Create the benchmark script** in this directory
2. **Add a description** to this README
3. **Include proper error handling** and logging
4. **Save results** in JSON format for analysis
5. **Update this README** with usage instructions

## Benchmark Structure

```python
#!/usr/bin/env python3
"""
Benchmark: [Brief description]

This benchmark tests [specific functionality].
"""

import json
import time
from typing import Dict, Any

def run_benchmark() -> Dict[str, Any]:
    """Run the benchmark and return results."""
    results = {
        "benchmark_name": "example_benchmark",
        "timestamp": time.time(),
        "results": []
    }

    # Your benchmark code here

    return results

def save_results(results: Dict[str, Any], filename: str) -> None:
    """Save benchmark results to JSON file."""
    with open(filename, 'w') as f:
        json.dump(results, f, indent=2)

if __name__ == "__main__":
    results = run_benchmark()
    save_results(results, "benchmark_results.json")
```
