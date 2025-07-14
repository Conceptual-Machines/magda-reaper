import os
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import MIDIResult
from ..prompt_loader import get_prompt
from ..utils import assess_task_complexity, retry_with_backoff, select_model_for_task
from .base import BaseAgent

load_dotenv()


class MidiAgent(BaseAgent):
    """Agent responsible for handling MIDI operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "midi"
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"),
            timeout=APIConfig.SPECIALIZED_AGENT_TIMEOUT,
        )

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle MIDI operations."""
        return operation.lower() in ["midi", "note", "chord", "delete note", "velocity"]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute MIDI operation using LLM with context awareness."""
        context = context or {}

        # Use LLM to parse MIDI operation and generate DAW command
        midi_info = self._parse_midi_operation_with_llm(operation)

        # Create MIDI result
        midi_result = MIDIResult(
            track_name=midi_info.get("track_name", "unknown"),
            operation=midi_info.get("operation", "note"),
            quantization=midi_info.get("quantization"),
            transpose_semitones=midi_info.get("transpose_semitones"),
            velocity=midi_info.get("velocity", 100),
            track_id=midi_info.get("track_id"),
            note=midi_info.get("note"),
            duration=midi_info.get("duration", 1.0),
            start_bar=midi_info.get("start_bar", 1),
            channel=midi_info.get("channel", 1),
        )

        # Generate DAW command
        daw_command = self._generate_daw_command(midi_result)

        return {
            "daw_command": daw_command,
            "result": midi_result.model_dump(),
            "context": context,
        }

    def _parse_midi_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse MIDI operation and extract parameters using dynamic model selection and retry logic."""

        instructions = get_prompt("midi_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(operation)
        selected_model = select_model_for_task("specialized", complexity, operation)

        print(
            f"MIDI agent - Task complexity: {complexity}, using model: {selected_model}"
        )

        def _make_api_call() -> dict[str, Any]:
            """Make the API call with retry logic."""
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": operation,
                "text_format": MIDIResult,
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
            print(f"All retries failed for MIDI agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["specialized"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['specialized']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["specialized"],
                        "instructions": instructions,
                        "input": operation,
                        "text_format": MIDIResult,
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

    def _generate_daw_command(self, midi: MIDIResult) -> str:
        """Generate DAW command string from MIDI result."""
        params = []

        # Track parameter
        track_name = midi.track_name or midi.track_id or "unknown"
        params.append(f"track:{track_name}")

        # Note
        if midi.note:
            params.append(f"note:{midi.note}")

        # Velocity
        if midi.velocity is not None:
            params.append(f"velocity:{midi.velocity}")

        # Duration
        if midi.duration is not None:
            params.append(f"duration:{midi.duration}")

        # Bar
        if midi.start_bar is not None:
            params.append(f"bar:{midi.start_bar}")

        # Channel
        if midi.channel is not None:
            params.append(f"channel:{midi.channel}")

        return f"midi({', '.join(params)})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["midi", "note", "chord", "delete note", "velocity"]
