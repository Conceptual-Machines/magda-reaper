import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..models import AgentResponse, VolumeResult
from .base import BaseAgent

load_dotenv()


class VolumeAgent(BaseAgent):
    """Agent responsible for handling volume automation operations using LLM."""

    def __init__(self):
        super().__init__()
        self.name = "volume"
        self.volume_automations = {}
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
        """Execute volume automation operation using LLM."""
        context = context or {}

        # Use LLM to parse volume operation and extract parameters
        volume_info = self._parse_volume_operation_with_llm(operation)

        # Get track information from context
        track_id = context.get("track_id")
        if not track_id:
            # Try to get track from context
            track_context = context.get("track")
            if track_context and "id" in track_context:
                track_id = track_context["id"]

        # Generate unique volume automation ID
        volume_id = str(uuid.uuid4())

        # Create volume result
        volume_result = VolumeResult(
            id=volume_id,
            track_id=track_id or "unknown",
            start_value=volume_info.get("start_value", 0.0),
            end_value=volume_info.get("end_value", 1.0),
            start_bar=volume_info.get("start_bar", 1),
            end_bar=volume_info.get("end_bar", volume_info.get("start_bar", 1) + 4),
        )

        # Store volume automation for future reference
        self.volume_automations[volume_id] = volume_result.dict()

        # Generate DAW command
        daw_command = self._generate_daw_command(volume_result)

        response = AgentResponse(
            result=volume_result.dict(), daw_command=daw_command, context=context
        )

        return response.dict()

    def _parse_volume_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse volume operation and extract parameters using Responses API."""

        instructions = """You are a volume automation specialist for a DAW system.
        Your job is to parse volume automation requests and extract the necessary parameters.

        Extract the following information:
        - start_value: The starting volume value (0.0 to 1.0, default: 0.0)
        - end_value: The ending volume value (0.0 to 1.0, default: 1.0)
        - start_bar: The starting bar number (default: 1)
        - end_bar: The ending bar number (default: start_bar + 4)

        Return a JSON object with the extracted parameters following the provided schema."""

        try:
            response = self.client.responses.parse(
                model="gpt-4.1",
                instructions=instructions,
                input=operation,
                text_format=VolumeResult,
                temperature=0.1,
            )

            # The parse method returns the parsed object directly
            return response.output_parsed.dict()

        except Exception as e:
            print(f"Error parsing volume operation: {e}")
            return {}

    def _generate_daw_command(self, volume: VolumeResult) -> str:
        """Generate DAW command string from volume result."""
        return f"volume(track:{volume.track_id}, start:{volume.start_bar}, end:{volume.end_bar}, start_value:{volume.start_value}, end_value:{volume.end_value})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["volume", "fade", "automation", "fade in", "fade out"]
