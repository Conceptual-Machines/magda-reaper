#!/usr/bin/env python3
"""
Shared prompt and schema loader for MAGDA.

This module provides utilities to load shared prompts and schemas
that are used by both Python and C++ implementations.
"""

import json
from pathlib import Path
from typing import Any


class SharedResources:
    """Load and manage shared prompts and schemas."""

    def __init__(self, base_path: str | None = None):
        """Initialize with path to shared resources."""
        if base_path is None:
            # Try to find the shared directory relative to this file
            current_file = Path(__file__)
            self.base_path = current_file.parent.parent
        else:
            self.base_path = Path(base_path)

    def load_prompt(self, prompt_name: str) -> str:
        """Load a prompt from the shared prompts directory."""
        prompt_path = self.base_path / "prompts" / f"{prompt_name}.md"

        if not prompt_path.exists():
            raise FileNotFoundError(f"Prompt file not found: {prompt_path}")

        return prompt_path.read_text(encoding="utf-8")

    def load_schema(self, schema_name: str) -> dict[str, Any]:
        """Load a JSON schema from the shared schemas directory."""
        schema_path = self.base_path / "schemas" / f"{schema_name}.json"

        if not schema_path.exists():
            raise FileNotFoundError(f"Schema file not found: {schema_path}")

        return json.loads(schema_path.read_text(encoding="utf-8"))

    def get_operation_identifier_prompt(self) -> str:
        """Get the operation identifier system prompt."""
        return self.load_prompt("operation_identifier")

    def get_track_agent_prompt(self) -> str:
        """Get the track agent system prompt."""
        return self.load_prompt("track_agent")

    def get_effect_agent_prompt(self) -> str:
        """Get the effect agent system prompt."""
        return self.load_prompt("effect_agent")

    def get_volume_agent_prompt(self) -> str:
        """Get the volume agent system prompt."""
        return self.load_prompt("volume_agent")

    def get_midi_agent_prompt(self) -> str:
        """Get the MIDI agent system prompt."""
        return self.load_prompt("midi_agent")

    def get_clip_agent_prompt(self) -> str:
        """Get the clip agent system prompt."""
        return self.load_prompt("clip_agent")

    def get_daw_operation_schema(self) -> dict[str, Any]:
        """Get the DAW operation JSON schema."""
        return self.load_schema("daw_operation")


# Convenience function for quick access
def get_shared_resources(base_path: str | None = None) -> SharedResources:
    """Get a SharedResources instance."""
    return SharedResources(base_path)


if __name__ == "__main__":
    # Test the loader
    resources = SharedResources()

    print("=== Operation Identifier Prompt ===")
    print(resources.get_operation_identifier_prompt())

    print("\n=== DAW Operation Schema ===")
    print(json.dumps(resources.get_daw_operation_schema(), indent=2))
