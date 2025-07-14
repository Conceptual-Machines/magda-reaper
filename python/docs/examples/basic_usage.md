# Basic Usage Examples

Simple examples to get you started with MAGDA.

## Understanding the Output Structure

Before diving into specific examples, let's understand what MAGDA returns:

```python
from magda import MAGDAPipeline

pipeline = MAGDAPipeline()
result = pipeline.process_prompt("Create a new track called 'Bass'")

# The result is a dictionary with the following structure:
print(f"Complete result: {result}")
# Output: Complete result: {
#   'success': True,                    # Whether the operation succeeded
#   'commands': ['track(name:Bass, id:uuid-1234-5678)'],  # Generated commands
#   'daw_commands': ['track(name:Bass, id:uuid-1234-5678)'],  # DAW-specific commands
#   'complexity': 'simple',             # Detected complexity level
#   'model_used': 'gpt-4o-mini'        # AI model that was used
# }

# You can access specific parts:
print(f"Success: {result['success']}")
print(f"Commands: {result['commands']}")
print(f"DAW Commands: {result['daw_commands']}")
print(f"Complexity: {result['complexity']}")
print(f"Model Used: {result['model_used']}")
```

## Basic Pipeline Usage

### Simple Track Creation

```python
from magda import MAGDAPipeline

# Initialize the pipeline
pipeline = MAGDAPipeline()

# Create a new track
result = pipeline.process_prompt("Create a new track called 'Bass'")
print(f"Result: {result}")
# Output: Result: {
#   'success': True,
#   'commands': ['track(name:Bass, id:uuid-1234-5678)'],
#   'daw_commands': ['track(name:Bass, id:uuid-1234-5678)'],
#   'complexity': 'simple',
#   'model_used': 'gpt-4o-mini'
# }

print(f"DAW Commands: {result['daw_commands']}")
# Output: DAW Commands: ['track(name:Bass, id:uuid-1234-5678)']
```

### Volume Control

```python
# Set volume
result = pipeline.process_prompt("Set the volume of track 'Guitar' to -6dB")
result['daw_commands']
# Output: ['volume(track:Guitar, start:1, end:5, start_value:-6.0, end_value:-6.0)']

# Mute a track
result = pipeline.process_prompt("Mute the drums track")
result['daw_commands']
# Output: ['volume(track:drums, start:1, end:5)']
```

### Effect Processing

```python
# Add reverb
result = pipeline.process_prompt("Add reverb to the vocals")
print(f"Full result: {result}")
# Output: Full result: {
#   'success': True,
#   'commands': ['effect(track:vocals, type:reverb, position:insert)'],
#   'daw_commands': ['effect(track:vocals, type:reverb, position:insert)'],
#   'complexity': 'simple',
#   'model_used': 'gpt-4o-mini'
# }

result['daw_commands']
# Output: ['effect(track:vocals, type:reverb, position:insert)']

# Add compression
result = pipeline.process_prompt("Add compression to the bass")
result['daw_commands']
# Output: ['effect(track:bass, type:compression, position:insert)']
```

### MIDI Operations

```python
# Create a clip with chord
result = pipeline.process_prompt("Create a 4-bar clip with a C major chord")
result['daw_commands']
# Output: ['midi(track:unknown, note:C,E,G, velocity:100, duration:2.0, bar:1, channel:1)']

# Quantize notes
result = pipeline.process_prompt("Quantize the MIDI notes to 16th notes")
result['daw_commands']
# Output: ['midi(track:unknown, quantize:16th)']
```

## Complex Operations

### Multiple Operations

```python
# Complex prompt with multiple operations
result = pipeline.process_prompt("Create a bass track, add compression, and set volume to -3dB")
print(f"Full result: {result}")
# Output: Full result: {
#   'success': True,
#   'commands': [
#     'track(name:bass, id:uuid-1234-5678)',
#     'effect(track:bass, type:compression, position:insert)',
#     'volume(track:bass, start:1, end:5, start_value:-3.0, end_value:-3.0)'
#   ],
#   'daw_commands': [
#     'track(name:bass, id:uuid-1234-5678)',
#     'effect(track:bass, type:compression, position:insert)',
#     'volume(track:bass, start:1, end:5, start_value:-3.0, end_value:-3.0)'
#   ],
#   'complexity': 'complex',
#   'model_used': 'gpt-4o-2024-07-18'
# }

result['daw_commands']
# Output: [
#   'track(name:bass, id:uuid-1234-5678)',
#   'effect(track:bass, type:compression, position:insert)',
#   'volume(track:bass, start:1, end:5, start_value:-3.0, end_value:-3.0)'
# ]
```

### Conditional Operations

```python
# Conditional logic
result = pipeline.process_prompt("If the guitar track exists, add reverb, otherwise create it first")
print(f"Full result: {result}")
# Output: Full result: {
#   'success': True,
#   'commands': [
#     'track(name:guitar, id:uuid-5678-9012)',
#     'effect(track:guitar, type:reverb, position:insert)'
#   ],
#   'daw_commands': [
#     'track(name:guitar, id:uuid-5678-9012)',
#     'effect(track:guitar, type:reverb, position:insert)'
#   ],
#   'complexity': 'complex',
#   'model_used': 'gpt-4o-2024-07-18'
# }

result['daw_commands']
# Output: ['track(name:guitar, id:uuid-5678-9012)', 'effect(track:guitar, type:reverb, position:insert)']
```

## Multi-Language Examples

### Japanese

```python
# Japanese prompt
result = pipeline.process_prompt("トラック「ベース」の音量を-6dBに設定してください")
result['daw_commands']
# Output: ['volume(track:ベース, start:1, end:5, start_value:-6.0, end_value:-6.0)']
```

### Spanish

```python
# Spanish prompt
result = pipeline.process_prompt("Crear una nueva pista llamada 'Guitarra'")
result['daw_commands']
# Output: ['track(name:Guitarra, id:uuid-1234-5678)']
```

### French

```python
# French prompt
result = pipeline.process_prompt("Ajouter de la réverbération à la piste de batterie")
result['daw_commands']
# Output: ['effect(track:batterie, type:reverb, position:insert)']
```

## Error Handling

### Invalid Prompts

```python
try:
    result = pipeline.process_prompt("Invalid prompt that doesn't make sense")
    print(f"Success: {result['daw_commands']}")
except Exception as e:
    print(f"Error: {str(e)}")
```

### Timeout Handling

```python
import signal
from magda.config import APIConfig

# Configure shorter timeout for testing
APIConfig.TOTAL_TIMEOUT = 30

try:
    result = pipeline.process_prompt("Create a very complex operation that might timeout")
except TimeoutError:
    print("Operation timed out")
```

## Batch Processing

### Multiple Prompts

```python
prompts = [
    "Create a new track called 'Bass'",
    "Set the volume to -6dB",
    "Add reverb",
    "Create a 4-bar clip"
]

results = []
for prompt in prompts:
    try:
        result = pipeline.process_prompt(prompt)
        results.append(result)
        print(f"✓ {prompt}: {result['daw_commands']}")
    except Exception as e:
        print(f"✗ {prompt}: {e}")
        results.append({'error': str(e)})
```

## Performance Monitoring

### Timing Operations

```python
import time

start_time = time.time()
result = pipeline.process_prompt("Create a new track and add reverb")
end_time = time.time()

print(f"Processing time: {end_time - start_time:.2f} seconds")
print(f"Commands: {result['daw_commands']}")
```

### Complexity Analysis

```python
from magda import get_complexity_detector

detector = get_complexity_detector()

prompts = [
    "Create a track",
    "Add reverb to bass and set volume",
    "Create complex automation with multiple effects"
]

for prompt in prompts:
    complexity = detector.detect_complexity(prompt)
    print(f"'{prompt}': {complexity.complexity} (confidence: {complexity.confidence:.2f})")
```

## CLI Usage

### Command Line Interface

```bash
# Basic usage
magda "Create a new track called 'Bass'"

# Multiple commands
magda "Create bass track, add compression, set volume -6dB"

# Japanese prompt
magda "トラック「ベース」の音量を-6dBに設定してください"
```

### Script Usage

```bash
#!/bin/bash
# process_commands.sh

echo "Processing DAW commands..."

magda "Create a new track called 'Drums'"
magda "Add compression to the drums track"
magda "Set drums volume to -3dB"
magda "Create a 4-bar clip with kick drum pattern"

echo "Done!"
```

## Next Steps

- Check out [Domain Agnostic Examples](domain_agnostic.md) for advanced usage
- Explore the [API Reference](../api/) for detailed documentation
- Learn about the [Architecture](../development/architecture.md) to understand how MAGDA works
