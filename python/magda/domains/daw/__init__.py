"""
DAW (Digital Audio Workstation) domain implementation for MAGDA.

This module implements the DAW-specific agents, orchestrator, and pipeline
using the domain-agnostic interfaces defined in magda.core.domain.
"""

from .daw_agents import (
    DAWClipAgent,
    DAWEffectAgent,
    DAWMidiAgent,
    DAWTrackAgent,
    DAWVolumeAgent,
)
from .daw_factory import DAWFactory
from .daw_orchestrator import DAWOrchestrator
from .daw_pipeline import DAWPipeline

__all__ = [
    "DAWTrackAgent",
    "DAWVolumeAgent",
    "DAWEffectAgent",
    "DAWClipAgent",
    "DAWMidiAgent",
    "DAWOrchestrator",
    "DAWPipeline",
    "DAWFactory",
]
