"""
Core domain-agnostic interfaces for MAGDA.

This module defines the abstract base classes and interfaces that make MAGDA
domain-agnostic, allowing any domain (DAW, Desktop, Web, etc.) to be implemented.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum
from typing import Any


class DomainType(Enum):
    """Supported domain types."""

    DAW = "daw"
    DESKTOP = "desktop"
    WEB = "web"
    MOBILE = "mobile"
    CLOUD = "cloud"
    BUSINESS = "business"


@dataclass
class DomainContext:
    """Context information for a specific domain."""

    domain_type: DomainType
    domain_config: dict[str, Any]
    host_context: dict[
        str, Any
    ]  # Host-provided context (e.g., VST plugins, track names)
    session_state: dict[str, Any]  # Session-specific state


@dataclass
class Operation:
    """Generic operation that can be applied to any domain."""

    operation_type: str
    description: str
    parameters: dict[str, Any]
    domain_specific_data: dict[str, Any] = None


@dataclass
class OperationResult:
    """Result of an operation execution."""

    success: bool
    command: str  # Domain-specific command (e.g., DAW command, API call)
    result_data: dict[str, Any]
    error_message: str | None = None


class DomainAgent(ABC):
    """Abstract base class for domain-specific agents."""

    def __init__(self, domain_type: DomainType):
        self.domain_type = domain_type
        self.agent_id: str = None

    @abstractmethod
    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle the given operation."""
        pass

    @abstractmethod
    def execute(self, operation: str, context: DomainContext) -> OperationResult:
        """Execute the operation and return the result."""
        pass

    @abstractmethod
    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        pass


class DomainOrchestrator(ABC):
    """Abstract base class for domain-specific orchestrators."""

    def __init__(self, domain_type: DomainType):
        self.domain_type = domain_type

    @abstractmethod
    def identify_operations(
        self, prompt: str, context: DomainContext
    ) -> list[Operation]:
        """Identify operations from natural language prompt."""
        pass

    @abstractmethod
    def select_model_for_task(self, prompt: str, context: DomainContext) -> str:
        """Select appropriate model for the task."""
        pass


class DomainPipeline(ABC):
    """Abstract base class for domain-specific pipelines."""

    def __init__(self, domain_type: DomainType):
        self.domain_type = domain_type
        self.orchestrator: DomainOrchestrator = None
        self.agents: dict[str, DomainAgent] = {}
        self.context: DomainContext = None

    @abstractmethod
    def process_prompt(self, prompt: str) -> dict[str, Any]:
        """Process a natural language prompt through the domain pipeline."""
        pass

    @abstractmethod
    def register_agent(self, agent: DomainAgent) -> None:
        """Register an agent with the pipeline."""
        pass


class DomainFactory(ABC):
    """Abstract factory for creating domain-specific components."""

    @abstractmethod
    def create_orchestrator(self, domain_type: DomainType) -> DomainOrchestrator:
        """Create a domain-specific orchestrator."""
        pass

    @abstractmethod
    def create_agents(self, domain_type: DomainType) -> dict[str, DomainAgent]:
        """Create domain-specific agents."""
        pass

    @abstractmethod
    def create_pipeline(self, domain_type: DomainType) -> DomainPipeline:
        """Create a domain-specific pipeline."""
        pass
