# Domain Agnostic Examples

Advanced examples showing MAGDA's domain-agnostic architecture.

## Overview

MAGDA's domain-agnostic architecture allows you to create custom domains beyond just DAW automation. This section shows how to extend MAGDA for different use cases.

## Basic Domain Agnostic Usage

### Using the Core Pipeline

```python
from magda import DomainPipeline, DAWFactory

# Create a DAW domain factory
daw_factory = DAWFactory()

# Create a domain-agnostic pipeline
pipeline = DomainPipeline(daw_factory)

# Process prompts
result = pipeline.process_prompt("Create a new track")
print(result)
```

### Creating Custom Domains

```python
from magda.core.domain import DomainFactory, DomainType, DomainContext, DomainOrchestrator, DomainAgent

class ImageEditingFactory(DomainFactory):
    """Factory for image editing domain."""

    def create_orchestrator(self) -> DomainOrchestrator:
        return ImageEditingOrchestrator()

    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "filter": ImageFilterAgent(),
            "transform": ImageTransformAgent(),
            "effect": ImageEffectAgent()
        }

    def create_pipeline(self) -> DomainPipeline:
        return DomainPipeline(self)

    def create_context(self) -> DomainContext:
        return DomainContext(
            domain_type=DomainType.IMAGE,
            domain_config={},
            host_context={},
            session_state={}
        )

# Use the custom domain
image_factory = ImageEditingFactory()
pipeline = DomainPipeline(image_factory)
result = pipeline.process_prompt("Apply blur filter to the image")
```

## Custom Domain Examples

### Image Editing Domain

```python
from magda.core.domain import DomainAgent, DomainType, OperationResult, DomainContext

class ImageFilterAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.IMAGE)
        self.name = "filter"

    def can_handle(self, operation: str) -> bool:
        filter_keywords = ["blur", "sharpen", "noise", "smooth", "filter"]
        return any(keyword in operation.lower() for keyword in filter_keywords)

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Parse and execute image filter operation
        filter_info = self._parse_filter_operation(operation)

        return OperationResult(
            success=True,
            command=f"apply_filter(type:{filter_info['type']}, intensity:{filter_info['intensity']})",
            result_data=filter_info
        )

    def get_capabilities(self) -> list[str]:
        return ["blur", "sharpen", "noise_reduction", "smoothing"]

    def _parse_filter_operation(self, operation: str) -> dict:
        # Implementation would parse the operation
        return {"type": "blur", "intensity": 0.5}

class ImageTransformAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.IMAGE)
        self.name = "transform"

    def can_handle(self, operation: str) -> bool:
        transform_keywords = ["resize", "rotate", "crop", "flip", "scale"]
        return any(keyword in operation.lower() for keyword in transform_keywords)

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        transform_info = self._parse_transform_operation(operation)

        return OperationResult(
            success=True,
            command=f"transform(type:{transform_info['type']}, params:{transform_info['params']})",
            result_data=transform_info
        )

    def get_capabilities(self) -> list[str]:
        return ["resize", "rotate", "crop", "flip", "scale"]

    def _parse_transform_operation(self, operation: str) -> dict:
        return {"type": "resize", "params": {"width": 800, "height": 600}}
```

### Code Generation Domain

```python
class CodeGenerationFactory(DomainFactory):
    """Factory for code generation domain."""

    def create_orchestrator(self) -> DomainOrchestrator:
        return CodeGenerationOrchestrator()

    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "function": FunctionGeneratorAgent(),
            "class": ClassGeneratorAgent(),
            "test": TestGeneratorAgent()
        }

    def create_pipeline(self) -> DomainPipeline:
        return DomainPipeline(self)

    def create_context(self) -> DomainContext:
        return DomainContext(
            domain_type=DomainType.CODE,
            domain_config={"language": "python"},
            host_context={},
            session_state={}
        )

class FunctionGeneratorAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.CODE)
        self.name = "function"

    def can_handle(self, operation: str) -> bool:
        return "function" in operation.lower() or "def" in operation.lower()

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        function_info = self._parse_function_operation(operation)

        return OperationResult(
            success=True,
            command=f"generate_function(name:{function_info['name']}, params:{function_info['params']})",
            result_data=function_info
        )

    def get_capabilities(self) -> list[str]:
        return ["generate_function", "create_function", "write_function"]

    def _parse_function_operation(self, operation: str) -> dict:
        return {"name": "calculate_sum", "params": ["a", "b"]}
```

### Web Automation Domain

```python
class WebAutomationFactory(DomainFactory):
    """Factory for web automation domain."""

    def create_orchestrator(self) -> DomainOrchestrator:
        return WebAutomationOrchestrator()

    def create_agents(self) -> dict[str, DomainAgent]:
        return {
            "navigation": NavigationAgent(),
            "interaction": InteractionAgent(),
            "scraping": ScrapingAgent()
        }

    def create_pipeline(self) -> DomainPipeline:
        return DomainPipeline(self)

    def create_context(self) -> DomainContext:
        return DomainContext(
            domain_type=DomainType.WEB,
            domain_config={"browser": "chrome"},
            host_context={},
            session_state={}
        )

class NavigationAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.WEB)
        self.name = "navigation"

    def can_handle(self, operation: str) -> bool:
        nav_keywords = ["navigate", "go to", "visit", "open", "browse"]
        return any(keyword in operation.lower() for keyword in nav_keywords)

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        nav_info = self._parse_navigation_operation(operation)

        return OperationResult(
            success=True,
            command=f"navigate(url:{nav_info['url']}, wait:{nav_info['wait']})",
            result_data=nav_info
        )

    def get_capabilities(self) -> list[str]:
        return ["navigate", "go_to", "visit", "open_url"]

    def _parse_navigation_operation(self, operation: str) -> dict:
        return {"url": "https://example.com", "wait": 5}
```

## Multi-Domain Usage

### Switching Between Domains

```python
# DAW domain
daw_factory = DAWFactory()
daw_pipeline = DomainPipeline(daw_factory)
daw_result = daw_pipeline.process_prompt("Create a new track")

# Image editing domain
image_factory = ImageEditingFactory()
image_pipeline = DomainPipeline(image_factory)
image_result = image_pipeline.process_prompt("Apply blur filter")

# Code generation domain
code_factory = CodeGenerationFactory()
code_pipeline = DomainPipeline(code_factory)
code_result = code_pipeline.process_prompt("Create a function to calculate sum")
```

### Domain-Specific Context

```python
# DAW context with VST plugins
daw_context = DomainContext(
    domain_type=DomainType.DAW,
    domain_config={},
    host_context={
        "vst_plugins": ["serum", "addictive drums", "valhalla reverb"],
        "track_names": ["bass", "drums", "guitar"]
    },
    session_state={"tempo": 120, "key": "C"}
)

# Image context with available filters
image_context = DomainContext(
    domain_type=DomainType.IMAGE,
    domain_config={},
    host_context={
        "available_filters": ["blur", "sharpen", "noise_reduction"],
        "image_format": "JPEG"
    },
    session_state={"current_image": "photo.jpg"}
)
```

## Advanced Patterns

### Composite Operations

```python
class CompositeAgent(DomainAgent):
    """Agent that can handle operations requiring multiple sub-agents."""

    def __init__(self, sub_agents: dict[str, DomainAgent]):
        super().__init__(DomainType.DAW)
        self.sub_agents = sub_agents

    def can_handle(self, operation: str) -> bool:
        # Check if any sub-agent can handle this operation
        return any(agent.can_handle(operation) for agent in self.sub_agents.values())

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Find the appropriate sub-agent
        for agent_name, agent in self.sub_agents.items():
            if agent.can_handle(operation):
                return agent.execute(operation, context)

        return OperationResult(
            success=False,
            command="",
            result_data={},
            error_message="No sub-agent can handle this operation"
        )

    def get_capabilities(self) -> list[str]:
        # Combine capabilities from all sub-agents
        capabilities = []
        for agent in self.sub_agents.values():
            capabilities.extend(agent.get_capabilities())
        return capabilities
```

### Domain-Specific Orchestrators

```python
class CustomOrchestrator(DomainOrchestrator):
    """Custom orchestrator with domain-specific logic."""

    def __init__(self, domain_type: DomainType):
        super().__init__(domain_type)
        self.domain_specific_rules = self._load_domain_rules()

    def identify_operations(self, prompt: str, context: DomainContext) -> list[Operation]:
        # Domain-specific operation identification
        operations = []

        if self.domain_type == DomainType.DAW:
            operations = self._identify_daw_operations(prompt)
        elif self.domain_type == DomainType.IMAGE:
            operations = self._identify_image_operations(prompt)

        return operations

    def select_model_for_task(self, prompt: str, context: DomainContext) -> str:
        # Domain-specific model selection
        if self.domain_type == DomainType.DAW:
            return "gpt-4o"  # DAW tasks need more context
        elif self.domain_type == DomainType.IMAGE:
            return "gpt-4o-mini"  # Image tasks are simpler

        return "gpt-4o-mini"  # Default

    def _load_domain_rules(self) -> dict:
        # Load domain-specific rules and patterns
        return {
            "daw": {"complexity_threshold": 0.7},
            "image": {"complexity_threshold": 0.5}
        }

    def _identify_daw_operations(self, prompt: str) -> list[Operation]:
        # DAW-specific operation identification
        operations = []
        # Implementation here
        return operations

    def _identify_image_operations(self, prompt: str) -> list[Operation]:
        # Image-specific operation identification
        operations = []
        # Implementation here
        return operations
```

## Testing Custom Domains

### Unit Tests for Custom Agents

```python
import pytest
from magda.core.domain import DomainContext, DomainType

def test_image_filter_agent():
    agent = ImageFilterAgent()
    context = DomainContext(
        domain_type=DomainType.IMAGE,
        domain_config={},
        host_context={},
        session_state={}
    )

    # Test capability detection
    assert agent.can_handle("apply blur filter")
    assert not agent.can_handle("create track")

    # Test execution
    result = agent.execute("apply blur filter", context)
    assert result.success
    assert "apply_filter" in result.command

def test_custom_factory():
    factory = ImageEditingFactory()

    # Test component creation
    context = factory.create_context()
    assert context.domain_type == DomainType.IMAGE

    agents = factory.create_agents()
    assert "filter" in agents
    assert "transform" in agents
```

## Best Practices

### 1. Domain Separation

Keep domain-specific logic separate:

```python
# Good: Domain-specific agent
class DAWTrackAgent(DomainAgent):
    def __init__(self):
        super().__init__(DomainType.DAW)

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # DAW-specific logic only
        pass

# Bad: Mixed domain logic
class MixedAgent(DomainAgent):
    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Don't mix DAW and image logic
        if "track" in operation:
            # DAW logic
            pass
        elif "filter" in operation:
            # Image logic
            pass
```

### 2. Consistent Interfaces

Ensure all agents follow the same interface:

```python
class ConsistentAgent(DomainAgent):
    def __init__(self, domain_type: DomainType):
        super().__init__(domain_type)
        self.name = "consistent_agent"

    def can_handle(self, operation: str) -> bool:
        # Always return boolean
        return True

    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        # Always return OperationResult
        return OperationResult(
            success=True,
            command="command()",
            result_data={}
        )

    def get_capabilities(self) -> list[str]:
        # Always return list of strings
        return ["capability1", "capability2"]
```

### 3. Error Handling

Implement robust error handling:

```python
class RobustAgent(DomainAgent):
    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        try:
            # Try to execute
            result = self._execute_operation(operation, context)
            return OperationResult(
                success=True,
                command=result["command"],
                result_data=result["data"]
            )
        except ValueError as e:
            # Handle validation errors
            return OperationResult(
                success=False,
                command="",
                result_data={},
                error_message=f"Validation error: {e}"
            )
        except Exception as e:
            # Handle unexpected errors
            return OperationResult(
                success=False,
                command="",
                result_data={},
                error_message=f"Unexpected error: {e}"
            )
```

## Next Steps

- Explore the [Core API Reference](../api/core.md) for detailed interface documentation
- Check out [Basic Usage Examples](basic_usage.md) for simpler examples
- Learn about the [Architecture](../development/architecture.md) to understand the design principles
