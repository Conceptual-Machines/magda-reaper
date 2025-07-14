import os
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import VolumeResult
from ..prompt_loader import get_prompt
from ..utils import assess_task_complexity, retry_with_backoff, select_model_for_task
from .base import BaseAgent

load_dotenv()


class VolumeAgent(BaseAgent):
    """Agent responsible for handling volume operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "volume"
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"),
            timeout=APIConfig.SPECIALIZED_AGENT_TIMEOUT,
        )

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle volume operations."""
        return operation.lower() in [
            "volume",
            "set volume",
            "adjust volume",
            "mute",
            "unmute",
        ]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute volume operation using LLM with context awareness."""
        context = context or {}

        # Use LLM to parse volume operation and generate DAW command
        volume_info = self._parse_volume_operation_with_llm(operation)

        # Create volume result
        volume_result = VolumeResult(
            track_name=volume_info.get("track_name"),
            volume=volume_info.get("volume"),
            fade_type=volume_info.get("fade_type"),
            fade_duration=volume_info.get("fade_duration"),
            start_value=volume_info.get("start_value"),
            end_value=volume_info.get("end_value"),
            start_bar=volume_info.get("start_bar", 1),
            end_bar=volume_info.get("end_bar", 5),
        )

        # Generate DAW command
        daw_command = self._generate_daw_command(volume_result)

        return {
            "daw_command": daw_command,
            "result": volume_result.model_dump(),
            "context": context,
        }

    def _parse_volume_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse volume operation and extract parameters using dynamic model selection and retry logic."""

        instructions = get_prompt("volume_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(operation)
        selected_model = select_model_for_task("specialized", complexity, operation)

        print(
            f"Volume agent - Task complexity: {complexity}, using model: {selected_model}"
        )

        def _make_api_call() -> dict[str, Any]:
            """Make the API call with retry logic."""
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": operation,
                "text_format": VolumeResult,
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
            print(f"All retries failed for volume agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["specialized"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['specialized']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["specialized"],
                        "instructions": instructions,
                        "input": operation,
                        "text_format": VolumeResult,
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

    def _generate_daw_command(self, volume: VolumeResult) -> str:
        """Generate DAW command string from volume result."""
        params = []

        # Track parameter
        track_name = volume.track_name or "unknown"
        params.append(f"track:{track_name}")

        # Bar range
        start_bar = volume.start_bar or 1
        end_bar = volume.end_bar or 5
        params.append(f"start:{start_bar}")
        params.append(f"end:{end_bar}")

        # Volume values
        if volume.start_value is not None:
            params.append(f"start_value:{volume.start_value}")
        if volume.end_value is not None:
            params.append(f"end_value:{volume.end_value}")

        return f"volume({', '.join(params)})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["volume", "set volume", "adjust volume", "mute", "unmute"]
