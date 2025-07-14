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

    # First stage: Operation identification (cost-effective, fast)
    OPERATION_IDENTIFIER = "gpt-4.1-mini"

    # Second stage: Specialized agents (higher quality, structured output)
    SPECIALIZED_AGENTS = "gpt-4o-mini"

    # Alternative for specialized agents (cost-effective but still high quality)
    SPECIALIZED_AGENTS_MINI = "gpt-4.1-mini"

    # Fallback model for error cases
    FALLBACK = "gpt-4o-mini"

    # Current model choices (easy to change in one place)
    CURRENT_DECISION_AGENT = OPERATION_IDENTIFIER
    CURRENT_SPECIALIZED_AGENTS = SPECIALIZED_AGENTS


class APIConfig:
    """API configuration settings."""

    # Default temperature for LLM requests
    DEFAULT_TEMPERATURE = 0.1

    # Timeout for API requests (seconds)
    REQUEST_TIMEOUT = 30


class AgentConfig:
    """Agent-specific configuration."""

    # Default context for agents
    DEFAULT_CONTEXT: dict[str, Any] = {}

    # Maximum number of retries for failed operations
    MAX_RETRIES = 3

    # Delay between retries (seconds)
    RETRY_DELAY = 1.0
