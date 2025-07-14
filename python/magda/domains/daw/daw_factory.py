"""
DAW domain factory that creates domain-specific components.

This factory implements the domain-agnostic DomainFactory interface to create
DAW-specific orchestrators, agents, and pipelines.
"""

from ...core.domain import (
    DomainAgent,
    DomainFactory,
    DomainOrchestrator,
    DomainPipeline,
    DomainType,
)
from .daw_agents import (
    DAWClipAgent,
    DAWEffectAgent,
    DAWMidiAgent,
    DAWTrackAgent,
    DAWVolumeAgent,
)
from .daw_orchestrator import DAWOrchestrator
from .daw_pipeline import DAWPipeline


class DAWFactory(DomainFactory):
    """Factory for creating DAW-specific components."""

    def create_orchestrator(self, domain_type: DomainType) -> DomainOrchestrator:
        """Create a DAW-specific orchestrator."""
        if domain_type != DomainType.DAW:
            raise ValueError(f"Expected DAW domain type, got {domain_type}")
        return DAWOrchestrator()

    def create_agents(self, domain_type: DomainType) -> dict[str, DomainAgent]:
        """Create DAW-specific agents."""
        if domain_type != DomainType.DAW:
            raise ValueError(f"Expected DAW domain type, got {domain_type}")

        return {
            "track": DAWTrackAgent(),
            "volume": DAWVolumeAgent(),
            "effect": DAWEffectAgent(),
            "clip": DAWClipAgent(),
            "midi": DAWMidiAgent(),
        }

    def create_pipeline(self, domain_type: DomainType) -> DomainPipeline:
        """Create a DAW-specific pipeline."""
        if domain_type != DomainType.DAW:
            raise ValueError(f"Expected DAW domain type, got {domain_type}")

        return DAWPipeline()
