"""
Centralized configuration for MAGDA Python implementation.

This module contains all configuration constants used throughout the application,
including model selections, API settings, and other configuration values.
"""

from typing import Any


class ModelConfig:
    """Centralized model configuration for MAGDA agents.

    This ensures consistency across all agents and makes it easy to update
    model choices for different stages of the pipeline.
    """

    # Fast models for simple tasks (quick responses, lower cost)
    FAST_MODELS = {
        "orchestrator": "gpt-4.1-mini",  # Fast reasoning
        "specialized": "gpt-4.1-mini",  # Fast specialized tasks
    }

    # High-quality models for complex tasks (better reasoning, higher cost)
    QUALITY_MODELS = {
        "orchestrator": "o3-mini",  # Best reasoning
        "specialized": "gpt-4.1",  # Best specialized tasks
    }

    # Fallback models
    FALLBACK_MODELS = {
        "orchestrator": "gpt-4.1-mini",
        "specialized": "gpt-4.1-mini",
    }

    # Current default model choices
    ORCHESTRATOR_AGENT = "o3-mini"
    SPECIALIZED_AGENTS = "gpt-4o-mini"

    # Model selection thresholds
    COMPLEXITY_THRESHOLDS = {
        "simple": {
            "max_words": 20,
            "max_operations": 1,
            "languages": ["en"],  # Only English
        },
        "medium": {
            "max_words": 50,
            "max_operations": 3,
            "languages": ["en", "es", "fr"],  # Common languages
        },
        "complex": {
            "max_words": float("inf"),
            "max_operations": float("inf"),
            "languages": ["en", "es", "fr", "de", "ja"],  # All languages
        },
    }


class APIConfig:
    """API configuration settings."""

    # Default temperature for LLM requests
    DEFAULT_TEMPERATURE = 0.1

    # Short timeouts for better UX
    REQUEST_TIMEOUT = 10  # Reduced from 30

    # Connection timeout (seconds)
    CONNECT_TIMEOUT = 5  # Reduced from 10

    # Read timeout (seconds)
    READ_TIMEOUT = 10  # Reduced from 30

    # Total timeout for operations (seconds)
    TOTAL_TIMEOUT = 15  # Reduced from 60

    # Timeout for orchestrator operations (seconds)
    ORCHESTRATOR_TIMEOUT = 10  # Reduced from 25

    # Timeout for specialized agent operations (seconds)
    SPECIALIZED_AGENT_TIMEOUT = 10  # Reduced from 30


class AgentConfig:
    """Agent-specific configuration."""

    # Default context for agents
    DEFAULT_CONTEXT: dict[str, Any] = {}

    # Retry configuration
    MAX_RETRIES = 3
    RETRY_DELAY = 0.5  # Reduced from 1.0 for faster retries

    # Exponential backoff for retries
    RETRY_BACKOFF_FACTOR = 1.5

    # Maximum retry delay
    MAX_RETRY_DELAY = 2.0
