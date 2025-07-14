import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import ClipResult
from ..prompt_loader import get_prompt
from .base import BaseAgent

load_dotenv()


class ClipAgent(BaseAgent):
    """Agent responsible for handling clip creation operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "clip"
        self.created_clips: dict[str, Any] = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle clip operations."""
        return operation.lower() in ["clip", "create clip", "add clip"]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute clip creation operation using LLM."""
        context = context or {}

        # Use LLM to parse clip operation and extract parameters
        clip_info = self._parse_clip_operation_with_llm(operation)

        # Get track information from context
        track_id = context.get("track_id")
        if not track_id:
            # Try to get track from context
            track_context = context.get("track")
            if track_context and "id" in track_context:
                track_id = track_context["id"]

        # Generate unique clip ID
        clip_id = str(uuid.uuid4())

        # Create clip result
        clip_result = ClipResult(
            clip_id=clip_id,
            track_name=context.get("track_name", "unknown"),
            track_id=track_id or "unknown",
            start_time=clip_info.get("start_time"),
            duration=clip_info.get("duration"),
            start_bar=clip_info.get("start_bar", 1),
            end_bar=clip_info.get("end_bar", clip_info.get("start_bar", 1) + 4),
        )

        # Store clip for future reference
        self.created_clips[clip_id] = clip_result.model_dump()

        # Generate DAW command
        daw_command = self._generate_daw_command(clip_result)

        return {
            "daw_command": daw_command,
            "result": clip_result.model_dump(),
            "context": context,
        }

    def _parse_clip_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse clip operation and extract parameters using Responses API."""

        instructions = get_prompt("clip_agent")

        try:
            response = self.client.responses.parse(
                model=ModelConfig.SPECIALIZED_AGENTS,
                instructions=instructions,
                input=operation,
                text_format=ClipResult,
                temperature=APIConfig.DEFAULT_TEMPERATURE,
            )

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return {}
            return response.output_parsed.model_dump()

        except Exception as e:
            print(f"Error parsing clip operation: {e}")
            return {}

    def _generate_daw_command(self, clip: ClipResult) -> str:
        """Generate DAW command string from clip result."""
        return (
            f"clip(track:{clip.track_name}, start:{clip.start_bar}, end:{clip.end_bar})"
        )

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["clip", "create clip", "add clip"]

    def get_clip_by_id(self, clip_id: str) -> dict[str, Any] | None:
        """Get clip information by ID."""
        return self.created_clips.get(clip_id)

    def list_clips(self) -> list[dict[str, Any]]:
        """List all created clips."""
        return list(self.created_clips.values())

    def get_clips_for_track(self, track_id: str) -> list[dict[str, Any]]:
        """Get all clips for a specific track."""
        return [
            clip for clip in self.created_clips.values() if clip["track_id"] == track_id
        ]
