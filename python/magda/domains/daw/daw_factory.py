"""
DAW domain factory that creates domain-specific components.

This factory implements the domain-agnostic DomainFactory interface to create
DAW-specific orchestrators, agents, and pipelines.
"""

from ...agents.orchestrator_agent import OrchestratorAgent
from ...core.domain import (
    DomainAgent,
    DomainContext,
    DomainFactory,
    DomainOrchestrator,
    DomainPipeline,
    DomainType,
)
from ...pipeline import MAGDAPipeline
from .daw_agents import (
    DAWClipAgent,
    DAWEffectAgent,
    DAWMidiAgent,
    DAWTrackAgent,
    DAWVolumeAgent,
)


class DAWFactory(DomainFactory):
    """Factory for creating DAW-specific components."""

    def create_orchestrator(self) -> DomainOrchestrator:
        """Create a DAW-specific orchestrator."""
        return OrchestratorAgent()

    def create_agents(self) -> dict[str, DomainAgent]:
        """Create DAW-specific agents."""
        return {
            "track": DAWTrackAgent(),
            "volume": DAWVolumeAgent(),
            "effect": DAWEffectAgent(),
            "clip": DAWClipAgent(),
            "midi": DAWMidiAgent(),
        }

    def create_pipeline(self) -> DomainPipeline:
        """Create a DAW-specific pipeline."""
        return MAGDAPipeline()

    def create_context(self) -> DomainContext:
        """Create a DAW-specific context."""
        return DomainContext(
            domain_type=DomainType.DAW,
            domain_config={},
            host_context={},
            session_state={},
        )
