# Agents API Reference

Specialized agents for different types of operations in MAGDA.

## Overview

Agents are specialized components that handle specific types of operations. Each agent is responsible for:

- **Operation detection**: Determining if it can handle a given operation
- **Execution**: Processing the operation and generating commands
- **Capability reporting**: Providing information about supported operations

## Base Agent Interface

All agents inherit from `DomainAgent`:

```python
from magda.core.domain import DomainAgent, DomainType, OperationResult, DomainContext

class MyAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)
        self.name = "my_agent"

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle the operation."""
        return "my_operation" in operation.lower()

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        """Execute the operation and return the result."""
        # Implementation here
        return OperationResult(
            success=True,
            command="my_command()",
            result_data={"key": "value"}
        )

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["my_operation", "another_operation"]
```

## Available Agents

### Track Agent

Handles track creation and management.

```python
from magda.agents.track_agent import TrackAgent

agent = TrackAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['create track', 'add track', 'rename track', 'delete track']

# Execute operation
result = agent.execute("create track bass", context)
```

### Volume Agent

Handles volume control operations.

```python
from magda.agents.volume_agent import VolumeAgent

agent = VolumeAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['set volume', 'adjust volume', 'mute', 'unmute', 'pan']

# Execute operation
result = agent.execute("set volume -6db", context)
```

### Effect Agent

Handles effect processing operations.

```python
from magda.agents.effect_agent import EffectAgent

agent = EffectAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['add effect', 'compression', 'reverb', 'eq']

# Execute operation
result = agent.execute("add reverb", context)
```

### Clip Agent

Handles clip management operations.

```python
from magda.agents.clip_agent import ClipAgent

agent = ClipAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['create clip', 'move clip', 'duplicate clip', 'split clip']

# Execute operation
result = agent.execute("create 4 bar clip", context)
```

### MIDI Agent

Handles MIDI operations.

```python
from magda.agents.midi_agent import MidiAgent

agent = MidiAgent()

# Check capabilities
capabilities = agent.get_capabilities()
# ['add note', 'add chord', 'quantize', 'transpose']

# Execute operation
result = agent.execute("add c major chord", context)
```

### Orchestrator Agent

Coordinates between different agents and handles complex operations.

```python
from magda.agents.orchestrator_agent import OrchestratorAgent

agent = OrchestratorAgent()

# The orchestrator handles complex prompts that require multiple agents
result = agent.execute("create track and add reverb", context)
```

## Agent Lifecycle

### 1. Initialization

```python
agent = TrackAgent()
# Agent is created with default configuration
```

### 2. Capability Check

```python
if agent.can_handle("create track bass"):
    # Agent can handle this operation
    result = agent.execute("create track bass", context)
else:
    # Try another agent
    pass
```

### 3. Execution

```python
result = agent.execute(operation, context)

if result.success:
    print(f"Command: {result.command}")
    print(f"Data: {result.result_data}")
else:
    print(f"Error: {result.error_message}")
```

## Creating Custom Agents

### Example: Automation Agent

```python
from magda.core.domain import DomainAgent, DomainType, OperationResult, DomainContext

class AutomationAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)
        self.name = "automation"
        self.client = openai.OpenAI()  # For LLM processing

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle automation operations."""
        automation_keywords = [
            "automation", "automate", "envelope", "curve",
            "fade", "crossfade", "lfo", "modulation"
        ]
        return any(keyword in operation.lower() for keyword in automation_keywords)

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        """Execute automation operation."""
        try:
            # Parse the automation operation
            automation_info = self._parse_automation_operation(operation)

            # Generate command
            command = self._generate_automation_command(automation_info)

            return OperationResult(
                success=True,
                command=command,
                result_data=automation_info
            )
        except Exception as e:
            return OperationResult(
                success=False,
                command="",
                result_data={},
                error_message=str(e)
            )

    def get_capabilities(self) -> list[str]:
        """Return list of automation operations this agent can handle."""
        return [
            "create automation",
            "edit automation",
            "delete automation",
            "fade in",
            "fade out",
            "crossfade",
            "lfo modulation"
        ]

    def _parse_automation_operation(self, operation: str) -> dict:
        """Parse automation operation using LLM."""
        # Implementation would use LLM to parse the operation
        return {
            "type": "volume_automation",
            "track": "bass",
            "start_time": 0,
            "end_time": 10,
            "start_value": 0,
            "end_value": 1
        }

    def _generate_automation_command(self, automation_info: dict) -> str:
        """Generate automation command from parsed info."""
        return f"automation(track:{automation_info['track']}, type:{automation_info['type']})"
```

### Registering Custom Agents

```python
from magda.domains.daw.daw_factory import DAWFactory

class CustomDAWFactory(DAWFactory):
    def create_agents(self) -> dict[str, DomainAgent]:
        """Create DAW-specific agents including custom ones."""
        agents = super().create_agents()

        # Add custom automation agent
        agents["automation"] = AutomationAgent()

        return agents
```

## Agent Configuration

### Model Selection

Agents can be configured to use different models:

```python
from magda.config import APIConfig

# Configure agent-specific timeouts
APIConfig.SPECIALIZED_AGENT_TIMEOUT = 30  # seconds

# Configure model selection
APIConfig.DEFAULT_MODEL = "gpt-4o-mini"
APIConfig.COMPLEX_MODEL = "gpt-4o"
```

### Error Handling

```python
class RobustAgent(DomainAgent):
    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        try:
            # Try primary method
            return self._execute_primary(operation, context)
        except Exception as e1:
            try:
                # Try fallback method
                return self._execute_fallback(operation, context)
            except Exception as e2:
                return OperationResult(
                    success=False,
                    command="",
                    result_data={},
                    error_message=f"Primary: {e1}, Fallback: {e2}"
                )
```

## Testing Agents

### Unit Tests

```python
import pytest
from magda.agents.track_agent import TrackAgent
from magda.core.domain import DomainContext, DomainType

def test_track_agent_capabilities():
    agent = TrackAgent()
    capabilities = agent.get_capabilities()

    assert "create track" in capabilities
    assert "delete track" in capabilities

def test_track_agent_can_handle():
    agent = TrackAgent()

    assert agent.can_handle("create track bass")
    assert not agent.can_handle("set volume")

def test_track_agent_execute():
    agent = TrackAgent()
    context = DomainContext(
        domain_type=DomainType.DAW,
        domain_config={},
        host_context={},
        session_state={}
    )

    result = agent.execute("create track bass", context)

    assert result.success
    assert "track" in result.command
    assert "bass" in result.command
```

## Performance Considerations

### Caching

```python
class CachedAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)
        self._cache = {}

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Check cache first
        cache_key = hash(operation)
        if cache_key in self._cache:
            return self._cache[cache_key]

        # Execute and cache result
        result = self._execute_operation(operation, context)
        self._cache[cache_key] = result
        return result
```

### Batch Processing

```python
def batch_execute(agent: DomainAgent, operations: list[str], context: DomainContext) -> list[OperationResult]:
    """Execute multiple operations in batch."""
    results = []

    for operation in operations:
        if agent.can_handle(operation):
            result = agent.execute(operation, context)
            results.append(result)
        else:
            results.append(OperationResult(
                success=False,
                command="",
                result_data={},
                error_message="Agent cannot handle this operation"
            ))

    return results
```
