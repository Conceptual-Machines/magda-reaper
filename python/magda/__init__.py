"""
MAGDA - Multi Agent Domain Automation

A Python library for translating natural language prompts into domain-specific commands.
Supports multiple domains including DAW (Digital Audio Workstation) automation.

Example:
    >>> from magda import MAGDAPipeline
    >>> pipeline = MAGDAPipeline()
    >>> result = pipeline.process_prompt("Create a new track called 'Bass'")
    >>> print(result['commands'])
"""

from .complexity.semantic_detector import (
    SemanticComplexityDetector,
    get_complexity_detector,
)
from .core.domain import DomainContext, DomainFactory, DomainOrchestrator
from .core.pipeline import DomainPipeline
from .domains.daw.daw_factory import DAWFactory
from .pipeline import MAGDAPipeline

# Version
__version__ = "1.1.0"

# Public API
__all__ = [
    "MAGDAPipeline",
    "DomainPipeline",
    "DomainFactory",
    "DomainContext",
    "DomainOrchestrator",
    "DAWFactory",
    "SemanticComplexityDetector",
    "get_complexity_detector",
]
