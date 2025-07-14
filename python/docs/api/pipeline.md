# Pipeline API Reference

The main pipeline components for processing natural language prompts.

## MAGDAPipeline

The main entry point for MAGDA processing.

```python
from magda import MAGDAPipeline

pipeline = MAGDAPipeline()
result = pipeline.process_prompt("Create a new track called 'Bass'")
```

### Methods

#### `process_prompt(prompt: str) -> dict[str, Any]`

Process a natural language prompt and return the results.

**Parameters:**
- `prompt` (str): The natural language prompt to process

**Returns:**
- `dict[str, Any]`: Dictionary containing:
  - `commands` (list): Generated commands
  - `daw_commands` (list): DAW-specific commands
  - `success` (bool): Whether processing was successful
  - `error` (str, optional): Error message if failed

### Example

```python
from magda import MAGDAPipeline

pipeline = MAGDAPipeline()

# Process a simple prompt
result = pipeline.process_prompt("Create a new track called 'Guitar'")
print(result['daw_commands'])
# Output: ['track(name:Guitar, id:uuid-1234-5678)']

# Process a complex prompt
result = pipeline.process_prompt("Add reverb to the drums track and set volume to -6dB")
print(result['daw_commands'])
# Output: ['effect(track:drums, type:reverb)', 'volume(track:drums, level:-6.0)']
```

## Pipeline Configuration

### Model Selection

MAGDA automatically selects the best model based on:

- **Task complexity**: Simple, medium, or complex
- **Reasoning requirements**: Whether logical reasoning is needed
- **Operation type**: Track, volume, effect, clip, or MIDI operations

### Available Models

- `gpt-4o-mini`: Fast, cost-effective for simple tasks
- `gpt-4o`: Balanced performance for medium complexity
- `gpt-4o-2024-07-18`: Best performance for complex reasoning tasks

### Timeout Configuration

```python
from magda.config import APIConfig

# Configure timeouts
APIConfig.SPECIALIZED_AGENT_TIMEOUT = 30  # seconds
APIConfig.ORCHESTRATOR_TIMEOUT = 60       # seconds
APIConfig.TOTAL_TIMEOUT = 120             # seconds
```

## Pipeline Flow

1. **Fast-path routing**: Check if prompt can be handled directly
2. **Complexity assessment**: Determine task complexity using semantic analysis
3. **Model selection**: Choose appropriate AI model
4. **Operation identification**: Parse prompt into operations
5. **Agent execution**: Execute operations using specialized agents
6. **Result compilation**: Combine results into final output

### Fast-path Routing

Simple operations are routed directly to agents without AI processing:

```python
# These prompts use fast-path routing
"create track bass"
"set volume -6db"
"add reverb"
"mute drums"
```

### Semantic Complexity Detection

MAGDA uses sentence transformers for accurate complexity classification:

```python
from magda import get_complexity_detector

detector = get_complexity_detector()
result = detector.detect_complexity("Add reverb to the drums track")
print(f"Complexity: {result.complexity} (confidence: {result.confidence})")
# Output: Complexity: simple (confidence: 0.85)
```

## Error Handling

The pipeline includes comprehensive error handling:

```python
try:
    result = pipeline.process_prompt("Invalid prompt")
except Exception as e:
    print(f"Error: {e}")
    # Handle error appropriately
```

### Common Error Types

- **TimeoutError**: Operation took too long
- **ValueError**: Invalid prompt or parameters
- **RuntimeError**: Internal processing error
- **ImportError**: Missing dependencies

## Performance Optimization

### Caching

MAGDA caches semantic models and embeddings for better performance:

```python
# Models are cached automatically
detector1 = get_complexity_detector()
detector2 = get_complexity_detector()  # Uses cached model
```

### Batch Processing

For multiple prompts, consider batching:

```python
prompts = [
    "Create a new track called 'Bass'",
    "Set volume to -6dB",
    "Add reverb"
]

results = []
for prompt in prompts:
    result = pipeline.process_prompt(prompt)
    results.append(result)
```

## Debugging

Enable debug output for troubleshooting:

```python
import logging

# Enable debug logging
logging.basicConfig(level=logging.DEBUG)

# Process with debug output
result = pipeline.process_prompt("Create a new track")
```
