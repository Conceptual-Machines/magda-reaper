# MAGDA Architecture

This document describes the high-level architecture of MAGDA (Multi Agent Domain Automation).

## Overview

MAGDA is designed as a domain-agnostic system that can translate natural language prompts into domain-specific commands. The architecture is built around the principle of separation of concerns, with clear interfaces between components.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    MAGDA Architecture                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐    Natural Language    ┌─────────────┐    │
│  │             │ ◄─────────────────────► │             │    │
│  │    User     │                        │   MAGDA     │    │
│  │  Interface  │                        │  Pipeline   │    │
│  │             │                        │             │    │
│  └─────────────┘                        └─────────────┘    │
│                                    │                       │
│                                    │ Domain Factory        │
│                                    ▼                       │
│  ┌─────────────┐    Domain-Specific   ┌─────────────┐     │
│  │             │ ◄────────────────────► │             │     │
│  │   Domain    │    Components         │   Agents    │     │
│  │ (DAW, etc.) │                       │             │     │
│  │             │                       └─────────────┘     │
│  └─────────────┘                                │          │
│                                                 │          │
│  ┌─────────────┐    Commands/Results   ┌─────────────┐    │
│  │             │ ◄────────────────────► │             │    │
│  │   Output    │                        │ Complexity  │    │
│  │  System     │                        │ Detection   │    │
│  │             │                        │             │    │
│  └─────────────┘                        └─────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Pipeline

The main entry point for processing natural language prompts.

**Responsibilities:**
- Coordinate the processing flow
- Handle error management
- Manage timeouts and retries
- Provide a unified interface

**Key Classes:**
- `MAGDAPipeline`: Main pipeline implementation
- `DomainPipeline`: Domain-agnostic base pipeline

### 2. Domain Factory

Creates domain-specific components using the factory pattern.

**Responsibilities:**
- Create orchestrators for specific domains
- Create agents for specific domains
- Create pipelines for specific domains
- Provide domain context

**Key Classes:**
- `DomainFactory`: Abstract base factory
- `DAWFactory`: DAW-specific factory implementation

### 3. Orchestrator

Coordinates the processing of complex prompts.

**Responsibilities:**
- Identify operations from natural language
- Select appropriate models for tasks
- Coordinate between multiple agents
- Handle complex reasoning tasks

**Key Classes:**
- `DomainOrchestrator`: Abstract base orchestrator
- `OrchestratorAgent`: DAW-specific orchestrator

### 4. Agents

Specialized components that handle specific types of operations.

**Responsibilities:**
- Execute domain-specific operations
- Generate commands
- Handle operation-specific logic
- Report capabilities

**Key Classes:**
- `DomainAgent`: Abstract base agent
- `TrackAgent`: Track management operations
- `VolumeAgent`: Volume control operations
- `EffectAgent`: Effect processing operations
- `ClipAgent`: Clip management operations
- `MidiAgent`: MIDI operations

### 5. Complexity Detection

Intelligent system for determining task complexity and model selection.

**Responsibilities:**
- Analyze prompt complexity
- Select appropriate AI models
- Optimize for cost and performance
- Provide confidence scores

**Key Classes:**
- `SemanticComplexityDetector`: Advanced complexity detection
- `SimpleComplexityDetector`: Fallback complexity detection

## Data Flow

### 1. Prompt Processing Flow

```
User Prompt → Pipeline → Complexity Detection → Model Selection → Orchestrator → Agents → Commands
```

### 2. Detailed Flow

1. **Input**: Natural language prompt
2. **Fast-path Routing**: Check if prompt can be handled directly
3. **Complexity Assessment**: Determine task complexity using semantic analysis
4. **Model Selection**: Choose appropriate AI model based on complexity
5. **Operation Identification**: Parse prompt into operations
6. **Agent Execution**: Execute operations using specialized agents
7. **Result Compilation**: Combine results into final output

### 3. Error Handling Flow

```
Operation → Try Primary Method → Success
    ↓
Failure → Try Fallback Method → Success
    ↓
Failure → Return Error Result
```

## Domain Architecture

### Domain-Agnostic Core

The core system is designed to be domain-agnostic, with clear abstractions:

```python
# Abstract interfaces
class DomainFactory(ABC):
    @abstractmethod
    def create_orchestrator(self) -> DomainOrchestrator: pass

    @abstractmethod
    def create_agents(self) -> dict[str, DomainAgent]: pass

    @abstractmethod
    def create_pipeline(self) -> DomainPipeline: pass

class DomainAgent(ABC):
    @abstractmethod
    def can_handle(self, operation: str) -> bool: pass

    @abstractmethod
    def execute(self, operation: str, context: DomainContext) -> OperationResult: pass

    @abstractmethod
    def get_capabilities(self) -> list[str]: pass
```

### Domain-Specific Implementations

Each domain implements the abstract interfaces:

```python
# DAW Domain
class DAWFactory(DomainFactory):
    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "track": DAWTrackAgent(),
            "volume": DAWVolumeAgent(),
            "effect": DAWEffectAgent(),
            "clip": DAWClipAgent(),
            "midi": DAWMidiAgent(),
        }

# Future domains can follow the same pattern
class ImageEditingFactory(DomainFactory):
    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "filter": ImageFilterAgent(),
            "transform": ImageTransformAgent(),
            "effect": ImageEffectAgent(),
        }
```

## Model Selection Strategy

### Complexity-Based Selection

MAGDA uses a sophisticated model selection strategy:

1. **Semantic Analysis**: Use sentence transformers to analyze prompt complexity
2. **Confidence Scoring**: Calculate confidence in complexity assessment
3. **Model Mapping**: Map complexity levels to appropriate models
4. **Cost Optimization**: Balance performance vs. cost

### Model Hierarchy

```
Simple Tasks (confidence > 0.8)     → gpt-4o-mini
Medium Tasks (confidence 0.5-0.8)   → gpt-4o
Complex Tasks (confidence < 0.5)    → gpt-4o-2024-07-18
```

### Reasoning Detection

Additional model selection based on reasoning requirements:

```python
def requires_reasoning(prompt: str) -> bool:
    reasoning_keywords = [
        "if", "then", "else", "when", "unless", "while",
        "for all", "except", "only if", "otherwise"
    ]
    return any(keyword in prompt.lower() for keyword in reasoning_keywords)
```

## Performance Optimization

### 1. Fast-Path Routing

Simple operations bypass AI processing:

```python
def fast_path_route(prompt: str) -> str | None:
    # Direct keyword matching for simple operations
    if "create track" in prompt.lower():
        return "track"
    if "set volume" in prompt.lower():
        return "volume"
    return None
```

### 2. Caching

Semantic models and embeddings are cached:

```python
class SemanticComplexityDetector:
    _model_cache = {}  # Class-level cache
    _embeddings_cache = {}  # Embeddings cache
```

### 3. Batch Processing

Support for processing multiple prompts efficiently:

```python
def batch_process(prompts: list[str]) -> list[dict]:
    results = []
    for prompt in prompts:
        result = pipeline.process_prompt(prompt)
        results.append(result)
    return results
```

## Error Handling Strategy

### 1. Graceful Degradation

```python
def process_with_fallback(prompt: str) -> dict:
    try:
        # Try semantic complexity detection
        return semantic_detector.detect_complexity(prompt)
    except Exception:
        # Fallback to simple word-count based detection
        return simple_detector.detect_complexity(prompt)
```

### 2. Timeout Management

```python
def execute_with_timeout(func, timeout_seconds):
    import signal

    def timeout_handler(signum, frame):
        raise TimeoutError(f"Operation timed out after {timeout_seconds} seconds")

    old_handler = signal.signal(signal.SIGALRM, timeout_handler)
    signal.alarm(timeout_seconds)

    try:
        result = func()
        return result
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old_handler)
```

### 3. Retry Logic

```python
def retry_with_backoff(func, max_retries=3, base_delay=1.0):
    for attempt in range(max_retries + 1):
        try:
            return func()
        except Exception as e:
            if attempt == max_retries:
                raise e
            delay = base_delay * (2 ** attempt)
            time.sleep(delay)
```

## Security Considerations

### 1. Input Validation

```python
def validate_prompt(prompt: str) -> bool:
    # Check for malicious content
    if len(prompt) > 10000:  # Prevent extremely long prompts
        return False

    # Check for injection attempts
    dangerous_patterns = ["<script>", "javascript:", "data:"]
    if any(pattern in prompt.lower() for pattern in dangerous_patterns):
        return False

    return True
```

### 2. API Key Management

```python
import os
from dotenv import load_dotenv

load_dotenv()
api_key = os.getenv("OPENAI_API_KEY")

if not api_key:
    raise ValueError("OPENAI_API_KEY environment variable is required")
```

### 3. Rate Limiting

```python
import time
from functools import wraps

def rate_limit(calls_per_minute=60):
    def decorator(func):
        last_call_time = 0
        min_interval = 60.0 / calls_per_minute

        @wraps(func)
        def wrapper(*args, **kwargs):
            nonlocal last_call_time
            current_time = time.time()

            if current_time - last_call_time < min_interval:
                time.sleep(min_interval - (current_time - last_call_time))

            last_call_time = time.time()
            return func(*args, **kwargs)

        return wrapper
    return decorator
```

## Extensibility

### 1. Adding New Domains

To add a new domain:

1. Create domain directory: `magda/domains/your_domain/`
2. Implement domain-specific agents
3. Create domain factory
4. Add tests and documentation

### 2. Adding New Agents

To add a new agent:

1. Inherit from `DomainAgent`
2. Implement required methods
3. Register with domain factory
4. Add tests

### 3. Adding New Models

To add new model support:

1. Update model selection logic
2. Add model configuration
3. Update tests
4. Document model capabilities

## Testing Strategy

### 1. Unit Tests

- Test individual components in isolation
- Mock external dependencies
- Test edge cases and error conditions

### 2. Integration Tests

- Test component interactions
- Test end-to-end workflows
- Test with real API calls (in CI)

### 3. Performance Tests

- Benchmark critical paths
- Test with large datasets
- Monitor memory usage

### 4. Domain Tests

- Test domain-specific functionality
- Test domain switching
- Test domain-specific error handling

## Future Architecture Considerations

### 1. MCP Integration

Future integration with Model Context Protocol:

```
MAGDA → MCP Client → DAW/External Systems
```

### 2. C++ Backend

Performance-critical components could be implemented in C++:

```
Python Frontend → C++ Backend → High-Performance Operations
```

### 3. Distributed Processing

For large-scale deployments:

```
Load Balancer → Multiple MAGDA Instances → Shared State
```

### 4. Plugin System

Extensible plugin architecture:

```
MAGDA Core → Plugin Interface → Custom Extensions
```

## Conclusion

MAGDA's architecture is designed to be:

- **Modular**: Clear separation of concerns
- **Extensible**: Easy to add new domains and capabilities
- **Performant**: Optimized for speed and efficiency
- **Reliable**: Robust error handling and fallbacks
- **Maintainable**: Clean interfaces and documentation

This architecture provides a solid foundation for building a powerful, domain-agnostic automation system that can grow and adapt to new requirements.
