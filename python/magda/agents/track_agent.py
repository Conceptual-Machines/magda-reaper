import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import TrackResult
from ..prompt_loader import get_prompt
from ..utils import assess_task_complexity, retry_with_backoff, select_model_for_task
from .base import BaseAgent

load_dotenv()


class TrackAgent(BaseAgent):
    """Agent responsible for handling track creation operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "track"
        self.created_tracks: dict[str, Any] = {}
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"),
            timeout=APIConfig.SPECIALIZED_AGENT_TIMEOUT,
        )

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle track operations."""
        return operation.lower() in ["track", "create track", "add track"]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute track creation operation using LLM with context awareness."""
        context = context or {}
        # context_manager = context.get("context_manager")  # TODO: Use context manager for track resolution

        # Use LLM to parse track operation and generate DAW command
        track_info = self._parse_track_operation_with_llm(operation)

        # Generate unique track ID
        track_id = str(uuid.uuid4())

        # Forward all required fields from track_info to TrackResult
        track_result = TrackResult(
            track_id=track_id,
            track_name=track_info.get("name", f"track_{track_id[:8]}"),
            vst=track_info.get("vst"),
            type="track",
            instrument=track_info.get("vst"),
        )

        # Store track for future reference
        self.created_tracks[track_id] = track_result.model_dump()

        # Generate DAW command with proper track ID
        daw_command = self._generate_daw_command(track_result)

        return {
            "daw_command": daw_command,
            "result": track_result.model_dump(),
            "context": context,
        }

    def _parse_track_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse track operation and extract parameters using dynamic model selection and retry logic."""

        instructions = get_prompt("track_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(operation)
        selected_model = select_model_for_task("specialized", complexity, operation)

        print(
            f"Track agent - Task complexity: {complexity}, using model: {selected_model}"
        )

        def _make_api_call() -> dict[str, Any]:
            """Make the API call with retry logic."""
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": operation,
                "text_format": TrackResult,
            }

            # Only add temperature for models that support it
            if not selected_model.startswith("o3"):
                params["temperature"] = APIConfig.DEFAULT_TEMPERATURE

            response = self.client.responses.parse(**params)

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return {}
            return response.output_parsed.model_dump()

        # Use retry logic with exponential backoff
        try:
            result = retry_with_backoff(_make_api_call)
            return result
        except Exception as e:
            print(f"All retries failed for track agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["specialized"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['specialized']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["specialized"],
                        "instructions": instructions,
                        "input": operation,
                        "text_format": TrackResult,
                        "temperature": APIConfig.DEFAULT_TEMPERATURE,
                    }
                    response = self.client.responses.parse(**fallback_params)
                    if response.output_parsed is None:
                        return {}
                    return response.output_parsed.model_dump()
                except Exception as fallback_error:
                    print(f"Fallback model also failed: {fallback_error}")
                    return {}
            return {}

    def _generate_daw_command(self, track: TrackResult) -> str:
        """Generate DAW command string from track result."""
        params = []
        if track.track_name:
            params.append(f"name:{track.track_name}")
        if track.vst:
            params.append(f"vst:{track.vst}")
        if track.track_id:
            params.append(f"id:{track.track_id}")

        return f"track({', '.join(params)})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["track", "create track", "add track"]

    def get_track_by_id(self, track_id: str) -> dict[str, Any] | None:
        """Get track information by ID."""
        return self.created_tracks.get(track_id)

    def list_tracks(self) -> list[dict[str, Any]]:
        """List all created tracks."""
        return list(self.created_tracks.values())
