"""
Complexity detection module for MAGDA.

This module provides sophisticated complexity analysis for DAW commands
using semantic similarity and machine learning techniques.
"""

from .semantic_detector import (
    ComplexityResult,
    SemanticComplexityDetector,
    SimpleComplexityDetector,
    get_complexity_detector,
)

__all__ = [
    "ComplexityResult",
    "SemanticComplexityDetector",
    "SimpleComplexityDetector",
    "get_complexity_detector",
]
