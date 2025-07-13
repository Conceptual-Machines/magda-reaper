import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..models import EffectParameters, EffectResult
from ..prompt_loader import get_prompt
from .base import BaseAgent

load_dotenv()


class EffectAgent(BaseAgent):
    """Agent responsible for handling effect operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "effect"
        self.effects: dict[str, Any] = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle effect operations."""
        return operation.lower() in [
            "effect",
            "reverb",
            "delay",
            "compressor",
            "eq",
            "filter",
            "distortion",
        ]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute effect operation using LLM with context awareness."""
        context = context or {}
        context_manager = context.get("context_manager")

        # Use LLM to parse effect operation and extract parameters
        effect_info = self._parse_effect_operation_with_llm(operation)

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

        # Generate unique effect ID
        effect_id = str(uuid.uuid4())

        # Create effect result
        effect_result = EffectResult(
            track_name=None,
            track_id=track_id,
            effect_type=effect_info.get("effect_type", "reverb"),
            parameters=EffectParameters(
                wet_mix=0.5,
                dry_mix=0.5,
                threshold=-20.0,
                ratio=4.0,
                attack=0.01,
                release=0.1,
                decay=0.5,
                feedback=0.3,
                delay_time=0.5,
                frequency=1000.0,
                q_factor=1.0,
                gain=0.0,
                wet=0.5,
                dry=0.5,
            ),
            position=effect_info.get("position", "insert"),
        )

        # Store effect for future reference
        self.effects[effect_id] = effect_result.model_dump()

        # Generate DAW command
        daw_command = self._generate_daw_command(effect_result)

        return {
            "daw_command": daw_command,
            "result": effect_result.model_dump(),
            "context": context,
        }

    def _parse_effect_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse effect operation and extract parameters using Responses API."""

        instructions = get_prompt("effect_agent")

        try:
            response = self.client.responses.parse(
                model="gpt-4.1",
                instructions=instructions,
                input=operation,
                text_format=EffectResult,
                temperature=0.1,
            )

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return {}
            return response.output_parsed.model_dump()

        except Exception as e:
            print(f"Error parsing effect operation: {e}")
            return {}

    def _generate_daw_command(self, effect: EffectResult) -> str:
        """Generate DAW command string from effect result."""
        params = effect.parameters
        if params is None:
            return f"effect(track:{effect.track_id}, type:{effect.effect_type}, position:{effect.position})"

        params_str = f"wet_mix:{params.wet_mix}, dry_mix:{params.dry_mix}"
        if effect.effect_type == "compressor":
            params_str += f", threshold:{params.threshold}, ratio:{params.ratio}"
        elif effect.effect_type == "reverb":
            params_str += f", decay:{params.decay}"
        elif effect.effect_type == "delay":
            params_str += f", feedback:{params.feedback}"
        return f"effect(track:{effect.track_id}, type:{effect.effect_type}, position:{effect.position}, params:{{{params_str}}})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["effect", "reverb", "delay", "compressor", "eq", "filter", "distortion"]
