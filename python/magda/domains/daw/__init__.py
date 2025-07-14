"""
DAW (Digital Audio Workstation) domain implementation for MAGDA.

This module implements the DAW-specific agents and factory
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

__all__ = [
    "DAWTrackAgent",
    "DAWVolumeAgent",
    "DAWEffectAgent",
    "DAWClipAgent",
    "DAWMidiAgent",
    "DAWFactory",
]
