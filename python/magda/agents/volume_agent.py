import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import VolumeResult
from ..prompt_loader import get_prompt
from .base import BaseAgent

load_dotenv()


class VolumeAgent(BaseAgent):
    """Agent responsible for handling volume automation operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "volume"
        self.volume_automations: dict[str, Any] = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle volume operations."""
        return operation.lower() in [
            "volume",
            "fade",
            "automation",
            "fade in",
            "fade out",
        ]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute volume automation operation using LLM with context awareness."""
        context = context or {}
        context_manager = context.get("context_manager")

        # Use LLM to parse volume operation and extract parameters
        volume_info = self._parse_volume_operation_with_llm(operation)

        # Get track information from context
        track_id: str = "unknown"
        if context_manager:
            # Try to get track from context parameters
            if "track_id" in context:
                track_id = str(context["track_id"])
            elif "track_daw_id" in context:
                track_id = str(context["track_daw_id"])
        else:
            # Fallback to old context method
            track_id = context.get("track_id", "unknown")
            if track_id == "unknown":
                track_context = context.get("track")
                if track_context and "id" in track_context:
                    track_id = str(track_context["id"])

        # Generate unique volume automation ID
        volume_id = str(uuid.uuid4())

        # Create volume result
        volume_result = VolumeResult(
            track_name=None,
            volume=None,
            fade_type=None,
            fade_duration=None,
            start_value=volume_info.get("start_value", 0.0),
            end_value=volume_info.get("end_value", 1.0),
            start_bar=volume_info.get("start_bar", 1),
            end_bar=volume_info.get("end_bar", volume_info.get("start_bar", 1) + 4),
        )

        # Store volume automation for future reference
        self.volume_automations[volume_id] = volume_result.model_dump()

        # Generate DAW command
        daw_command = self._generate_daw_command(volume_result, track_id)

        return {
            "daw_command": daw_command,
            "result": volume_result.model_dump(),
            "context": context,
        }

    def _parse_volume_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse volume operation and extract parameters using Responses API."""

        instructions = get_prompt("volume_agent")

        try:
            response = self.client.responses.parse(
                model=ModelConfig.SPECIALIZED_AGENTS,
                instructions=instructions,
                input=operation,
                text_format=VolumeResult,
                temperature=APIConfig.DEFAULT_TEMPERATURE,
            )

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return {}
            return response.output_parsed.model_dump()

        except Exception as e:
            print(f"Error parsing volume operation: {e}")
            return {}

    def _generate_daw_command(self, volume: VolumeResult, track_id: str) -> str:
        """Generate DAW command string from volume result."""
        return f"volume(track:{track_id}, start:{volume.start_bar}, end:{volume.end_bar}, start_value:{volume.start_value}, end_value:{volume.end_value})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["volume", "fade", "automation", "fade in", "fade out"]
