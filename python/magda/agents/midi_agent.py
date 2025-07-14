import os
import uuid
from typing import Any

import openai
from dotenv import load_dotenv

from ..config import APIConfig, ModelConfig
from ..models import MIDIResult
from ..prompt_loader import get_prompt
from .base import BaseAgent

load_dotenv()


class MidiAgent(BaseAgent):
    """Agent responsible for handling MIDI operations using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.name = "midi"
        self.midi_events: dict[str, Any] = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle MIDI operations."""
        return operation.lower() in [
            "midi",
            "note",
            "chord",
            "sequence",
            "arpeggio",
            "melody",
        ]

    def execute(
        self, operation: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Execute MIDI operation using LLM."""
        context = context or {}

        # Use LLM to parse MIDI operation and extract parameters
        midi_info = self._parse_midi_operation_with_llm(operation)

        # Get track information from context
        track_id = context.get("track_id")
        if not track_id:
            # Try to get track from context
            track_context = context.get("track")
            if track_context and "id" in track_context:
                track_id = track_context["id"]

        # Generate unique MIDI event ID
        midi_id = str(uuid.uuid4())

        # Create MIDI result
        midi_result = MIDIResult(
            track_name=context.get("track_name", "unknown"),
            track_id=track_id or "unknown",
            operation=midi_info.get("operation", "note"),
            quantization=midi_info.get("quantization"),
            transpose_semitones=midi_info.get("transpose_semitones"),
            velocity=midi_info.get("velocity", 100),
            note=midi_info.get("note", "C4"),
            duration=midi_info.get("duration", 1.0),
            start_bar=midi_info.get("start_bar", 1),
            channel=midi_info.get("channel", 1),
        )

        # Store MIDI event for future reference
        self.midi_events[midi_id] = midi_result.model_dump()

        # Generate DAW command
        daw_command = self._generate_daw_command(midi_result)

        return {
            "daw_command": daw_command,
            "result": midi_result.model_dump(),
            "context": context,
        }

    def _parse_midi_operation_with_llm(self, operation: str) -> dict[str, Any]:
        """Use LLM to parse MIDI operation and extract parameters using Responses API."""

        instructions = get_prompt("midi_agent")

        try:
            response = self.client.responses.parse(
                model=ModelConfig.SPECIALIZED_AGENTS,
                instructions=instructions,
                input=operation,
                text_format=MIDIResult,
                temperature=APIConfig.DEFAULT_TEMPERATURE,
            )

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return {}
            return response.output_parsed.model_dump()

        except Exception as e:
            print(f"Error parsing MIDI operation: {e}")
            return {}

    def _generate_daw_command(self, midi: MIDIResult) -> str:
        """Generate DAW command string from MIDI result."""
        return f"midi(track:{midi.track_id}, note:{midi.note}, velocity:{midi.velocity}, duration:{midi.duration}, bar:{midi.start_bar}, channel:{midi.channel})"

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can handle."""
        return ["midi", "note", "chord", "sequence", "arpeggio", "melody"]
