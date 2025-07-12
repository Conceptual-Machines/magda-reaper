from abc import ABC, abstractmethod
from typing import Dict, Any, List
import uuid


class BaseAgent(ABC):
    """Base class for all MAGDA agents."""
    
    def __init__(self):
        self.agent_id = str(uuid.uuid4())
    
    @abstractmethod
    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle the given operation."""
        pass
    
    @abstractmethod
    def execute(self, operation: str, context: Dict[str, Any] = None) -> Dict[str, Any]:
        """Execute the operation and return the result."""
        pass
    
    def get_agent_info(self) -> Dict[str, str]:
        """Get information about this agent."""
        return {
            "agent_id": self.agent_id,
            "agent_type": self.__class__.__name__,
            "capabilities": self.get_capabilities()
        }
    
    @abstractmethod
    def get_capabilities(self) -> List[str]:
        """Return list of operations this agent can handle."""
        pass 