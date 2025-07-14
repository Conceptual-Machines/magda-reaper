import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import ClipResult
from ..prompt_loader import get_prompt
from ..utils import assess_task_complexity, retry_with_backoff, select_model_for_task
from .base import BaseAgent

load_dotenv()


class ClipAgent(BaseAgent):
    """Agent responsible for handling clip operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "clip"
        self.created_clips: dict[str, Any] = {}
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"),
            timeout=APIConfig.SPECIALIZED_AGENT_TIMEOUT,
        )

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle clip operations."""
        return operation.lower() in ["clip", "create clip", "delete clip", "move clip"]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute clip operation using LLM with context awareness."""
        context = context or {}

        # Use LLM to parse clip operation and generate DAW command
        clip_info = self._parse_clip_operation_with_llm(operation)

        # Generate unique clip ID
        clip_id = str(uuid.uuid4())

        # Get track information from context
        track_id = context.get("track_id", "unknown")
        track_name = context.get("track_name", "unknown")

        # Create clip result
        clip_result = ClipResult(
            clip_id=clip_id,
            track_name=clip_info.get("track_name", track_name),
            track_id=clip_info.get("track_id", track_id),
            start_time=clip_info.get("start_time"),
            duration=clip_info.get("duration"),
            start_bar=clip_info.get("start_bar", 1),
            end_bar=clip_info.get("end_bar", 4),
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
        """Use LLM to parse clip operation and extract parameters using dynamic model selection and retry logic."""

        instructions = get_prompt("clip_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(operation)
        selected_model = select_model_for_task("specialized", complexity, operation)

        print(
            f"Clip agent - Task complexity: {complexity}, using model: {selected_model}"
        )

        def _make_api_call() -> dict[str, Any]:
            """Make the API call with retry logic."""
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": operation,
                "text_format": ClipResult,
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
            print(f"All retries failed for clip agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["specialized"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['specialized']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["specialized"],
                        "instructions": instructions,
                        "input": operation,
                        "text_format": ClipResult,
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

    def _generate_daw_command(self, clip: ClipResult) -> str:
        """Generate DAW command string from clip result."""
        params = []

        # Track parameter
        track_name = clip.track_name or "unknown"
        params.append(f"track:{track_name}")

        # Bar range
        start_bar = clip.start_bar or 1
        end_bar = clip.end_bar or 4
        params.append(f"start:{start_bar}")
        params.append(f"end:{end_bar}")

        return f"clip({', '.join(params)})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["clip", "create clip", "delete clip", "move clip"]

    def get_clip_by_id(self, clip_id: str) -> dict[str, Any] | None:
        """Get clip information by ID."""
        return self.created_clips.get(clip_id)

    def list_clips(self) -> list[dict[str, Any]]:
        """List all created clips."""
        return list(self.created_clips.values())
