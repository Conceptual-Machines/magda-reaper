import uuid
from abc import ABC, abstractmethod
from typing import Any


class BaseAgent(ABC):
    """Base class for all MAGDA agents."""

    def __init__(self):
        self.agent_id = str(uuid.uuid4())

    @abstractmethod
    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle the given operation."""
        pass

    @abstractmethod
    def execute(self, operation: str, context: dict[str, Any] = None) -> dict[str, Any]:
        """Execute the operation and return the result."""
        pass

    def get_agent_info(self) -> dict[str, str]:
        """Get information about this agent."""
        return {
            "agent_id": self.agent_id,
            "agent_type": self.__class__.__name__,
            "capabilities": self.get_capabilities(),
        }

    @abstractmethod
    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        pass
