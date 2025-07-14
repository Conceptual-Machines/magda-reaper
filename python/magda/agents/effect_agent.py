import os
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import EffectResult
from ..prompt_loader import get_prompt
from ..utils import assess_task_complexity, retry_with_backoff, select_model_for_task
from .base import BaseAgent

load_dotenv()


class EffectAgent(BaseAgent):
    """Agent responsible for handling effect operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "effect"
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"),
            timeout=APIConfig.SPECIALIZED_AGENT_TIMEOUT,
        )

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle effect operations."""
        return operation.lower() in [
            "effect",
            "add effect",
            "remove effect",
            "reverb",
            "delay",
            "compressor",
        ]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute effect operation using LLM with context awareness."""
        context = context or {}

        # Use LLM to parse effect operation and generate DAW command
        effect_info = self._parse_effect_operation_with_llm(operation)

        # Create effect result
        effect_result = EffectResult(
            track_name=effect_info.get("track_name"),
            effect_type=effect_info.get("effect_type"),
            parameters=effect_info.get("parameters"),
            track_id=effect_info.get("track_id"),
            position=effect_info.get("position", "insert"),
        )

        # Generate DAW command
        daw_command = self._generate_daw_command(effect_result)

        return {
            "daw_command": daw_command,
            "result": effect_result.model_dump(),
            "context": context,
        }

    def _parse_effect_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse effect operation and extract parameters using dynamic model selection and retry logic."""

        instructions = get_prompt("effect_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(operation)
        selected_model = select_model_for_task("specialized", complexity, operation)

        print(
            f"Effect agent - Task complexity: {complexity}, using model: {selected_model}"
        )

        def _make_api_call() -> dict[str, Any]:
            """Make the API call with retry logic."""
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": operation,
                "text_format": EffectResult,
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
            print(f"All retries failed for effect agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["specialized"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['specialized']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["specialized"],
                        "instructions": instructions,
                        "input": operation,
                        "text_format": EffectResult,
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

    def _generate_daw_command(self, effect: EffectResult) -> str:
        """Generate DAW command string from effect result."""
        params = []

        # Track parameter
        track_name = effect.track_name or effect.track_id or "unknown"
        params.append(f"track:{track_name}")

        # Effect type
        if effect.effect_type:
            params.append(f"type:{effect.effect_type}")

        # Position
        if effect.position:
            params.append(f"position:{effect.position}")

        # Parameters
        if effect.parameters:
            # Convert EffectParameters model to dict and filter out None values
            param_dict = effect.parameters.model_dump(exclude_none=True)
            if param_dict:
                param_str = ",".join([f"{k}:{v}" for k, v in param_dict.items()])
                params.append(f"params:{{{param_str}}}")

        return f"effect({', '.join(params)})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return [
            "effect",
            "add effect",
            "remove effect",
            "reverb",
            "delay",
            "compressor",
        ]
