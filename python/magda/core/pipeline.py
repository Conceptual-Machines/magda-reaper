"""
Main domain-agnostic pipeline for MAGDA.

This pipeline can work with any domain implementation (DAW, Desktop, Web, etc.)
by using the domain-agnostic interfaces and factory pattern.
"""

import signal
from typing import Any

from ..config import APIConfig
from .domain import DomainContext, DomainFactory, DomainPipeline, DomainType


class MAGDACorePipeline:
    """Main domain-agnostic pipeline for MAGDA."""

    def __init__(self, domain_factory: DomainFactory, domain_type: DomainType):
        """
        Initialize the core pipeline.

        Args:
            domain_factory: Factory for creating domain-specific components
            domain_type: Type of domain to use (DAW, Desktop, Web, etc.)
        """
        self.domain_factory = domain_factory
        self.domain_type = domain_type
        self.domain_pipeline: DomainPipeline | None = None
        self.domain_context: DomainContext | None = None

        # Initialize domain-specific components
        self._initialize_domain()

    def _initialize_domain(self) -> None:
        """Initialize domain-specific components."""
        # Create domain-specific pipeline
        self.domain_pipeline = self.domain_factory.create_pipeline(self.domain_type)

        # Create domain context
        self.domain_context = DomainContext(
            domain_type=self.domain_type,
            domain_config=self._get_domain_config(),
            host_context={},  # Will be populated by host
            session_state={},  # Will be populated during session
        )

    def _get_domain_config(self) -> dict[str, Any]:
        """Get domain-specific configuration."""
        configs = {
            DomainType.DAW: {
                "supported_operations": ["track", "volume", "effect", "clip", "midi"],
                "fast_path_keywords": ["track", "volume", "effect", "clip", "midi"],
                "model_selection": "complexity_based",
            },
            DomainType.DESKTOP: {
                "supported_operations": ["file", "window", "app", "system"],
                "fast_path_keywords": ["file", "window", "app", "system"],
                "model_selection": "complexity_based",
            },
            DomainType.WEB: {
                "supported_operations": ["browser", "form", "click", "navigate"],
                "fast_path_keywords": ["browser", "form", "click", "navigate"],
                "model_selection": "complexity_based",
            },
            DomainType.MOBILE: {
                "supported_operations": ["app", "tap", "swipe", "input"],
                "fast_path_keywords": ["app", "tap", "swipe", "input"],
                "model_selection": "complexity_based",
            },
            DomainType.CLOUD: {
                "supported_operations": ["aws", "azure", "gcp", "deploy"],
                "fast_path_keywords": ["aws", "azure", "gcp", "deploy"],
                "model_selection": "complexity_based",
            },
            DomainType.BUSINESS: {
                "supported_operations": ["crm", "email", "calendar", "report"],
                "fast_path_keywords": ["crm", "email", "calendar", "report"],
                "model_selection": "complexity_based",
            },
        }

        return configs.get(self.domain_type, {})

    def set_host_context(self, host_context: dict[str, Any]) -> None:
        """Set host-provided context (e.g., VST plugins, track names)."""
        if self.domain_context:
            self.domain_context.host_context = host_context

    def update_session_state(self, session_state: dict[str, Any]) -> None:
        """Update session-specific state."""
        if self.domain_context:
            self.domain_context.session_state.update(session_state)

    def _execute_with_timeout(self, func, *args, **kwargs):
        """Execute function with timeout."""

        def timeout_handler(signum, frame):
            raise TimeoutError(
                f"Operation timed out after {APIConfig.TOTAL_TIMEOUT} seconds"
            )

        old_handler = signal.signal(signal.SIGALRM, timeout_handler)
        signal.alarm(APIConfig.TOTAL_TIMEOUT)

        try:
            return func(*args, **kwargs)
        finally:
            signal.signal(signal.SIGALRM, old_handler)

    def process_prompt(self, prompt: str) -> dict[str, Any]:
        """
        Process a natural language prompt through the domain pipeline.

        Args:
            prompt: Natural language prompt to process

        Returns:
            Dictionary containing results and commands
        """
        if not self.domain_pipeline or not self.domain_context:
            raise RuntimeError("Domain pipeline not initialized")

        try:
            # Process through domain-specific pipeline
            result = self._execute_with_timeout(
                self.domain_pipeline.process_prompt, prompt
            )

            # Add domain context to result
            result["domain_type"] = self.domain_type.value
            result["domain_context"] = {
                "host_context": self.domain_context.host_context,
                "session_state": self.domain_context.session_state,
            }

            return result

        except TimeoutError as e:
            return {
                "success": False,
                "error": f"Operation timed out: {e}",
                "domain_type": self.domain_type.value,
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Pipeline error: {e}",
                "domain_type": self.domain_type.value,
            }

    def get_domain_info(self) -> dict[str, Any]:
        """Get information about the current domain."""
        return {
            "domain_type": self.domain_type.value,
            "domain_config": self._get_domain_config(),
            "supported_operations": self._get_domain_config().get(
                "supported_operations", []
            ),
        }
