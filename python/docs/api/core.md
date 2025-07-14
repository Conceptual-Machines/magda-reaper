# Core API Reference

The core components of MAGDA's domain-agnostic architecture.

## DomainPipeline

The main domain-agnostic pipeline for processing natural language prompts.

```python
from magda import DomainPipeline, DAWFactory

factory = DAWFactory()
pipeline = DomainPipeline(factory)
result = pipeline.process_prompt("Create a new track")
```

### Methods

#### `process_prompt(prompt: str) -> dict[str, Any]`

Process a natural language prompt through the domain pipeline.

**Parameters:**
- `prompt` (str): The natural language prompt to process

**Returns:**
- `dict[str, Any]`: Dictionary containing results and commands

## DomainFactory

Abstract factory for creating domain-specific components.

```python
from magda import DomainFactory

class MyDomainFactory(DomainFactory):
    def create_orchestrator(self) -> DomainOrchestrator:
        # Implementation
        pass

    def create_agents(self) -> dict[str, DomainAgent]:
        # Implementation
        pass

    def create_pipeline(self) -> DomainPipeline:
        # Implementation
        pass

    def create_context(self) -> DomainContext:
        # Implementation
        pass
```

## DomainContext

Context information for a specific domain.

```python
from magda import DomainContext, DomainType

context = DomainContext(
    domain_type=DomainType.DAW,
    domain_config={},
    host_context={},
    session_state={}
)
```

### Properties

- `domain_type` (DomainType): The type of domain
- `domain_config` (dict): Domain-specific configuration
- `host_context` (dict): Host-provided context
- `session_state` (dict): Session-specific state

## DomainOrchestrator

Abstract base class for domain-specific orchestrators.

```python
from magda import DomainOrchestrator

class MyOrchestrator(DomainOrchestrator):
    def identify_operations(self, prompt: str, context: DomainContext) -> list[Operation]:
        # Implementation
        pass

    def select_model_for_task(self, prompt: str, context: DomainContext) -> str:
        # Implementation
        pass
```

## DomainAgent

Abstract base class for domain-specific agents.

```python
from magda import DomainAgent, DomainType

class MyAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)

    def can_handle(self, operation: str) -> bool:
        # Implementation
        pass

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Implementation
        pass

    def get_capabilities(self) -> list[str]:
        # Implementation
        pass
```

## OperationResult

Result of an operation execution.

```python
from magda import OperationResult

result = OperationResult(
    success=True,
    command="create_track('bass')",
    result_data={"track_id": "123", "track_name": "bass"},
    error_message=None
)
```

### Properties

- `success` (bool): Whether the operation was successful
- `command` (str): The generated command
- `result_data` (dict): Additional result data
- `error_message` (str, optional): Error message if failed

## DomainType

Enumeration of supported domain types.

```python
from magda import DomainType

# Available domain types
DomainType.DAW        # Digital Audio Workstation
DomainType.DESKTOP    # Desktop applications
DomainType.WEB        # Web applications
DomainType.MOBILE     # Mobile applications
DomainType.CLOUD      # Cloud services
DomainType.BUSINESS   # Business applications
```
