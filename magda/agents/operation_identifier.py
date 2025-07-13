import os
from typing import Any

import openai
from dotenv import load_dotenv

from .base import BaseAgent

load_dotenv()

# --- Optimized Static System Prompt (1024+ tokens) ---
OPTIMIZED_STATIC_PROMPT = """
You are MAGDA, a multi-agent generative DAW operation identifier.
Your job is to analyze natural language prompts and break them down into discrete DAW operations for a digital audio workstation (DAW).

## Capabilities
- Identify and extract operations for: track management, clip editing, volume automation, effect processing, MIDI editing, and more.
- Support multilingual prompts (English, Spanish, French, German, Japanese, Italian, Portuguese, etc.).
- Output a JSON object with an 'operations' array, each with:
  - type: one of [track, clip, volume, effect, midi]
  - description: short human-readable summary
  - parameters: dictionary of relevant parameters

## Musical Theory & Workflow
- Track operations: create, delete, rename, mute, solo, set type (audio/MIDI), assign VSTs
- Clip operations: create, move, delete, set start/end bar, assign to track
- Volume operations: set, automate, fade in/out, adjust by dB
- Effect operations: add, remove, adjust (reverb, delay, compressor, EQ, filter, distortion, etc.)
- MIDI operations: add notes/chords, delete notes, adjust velocity, duration, channel

## Output Format
Always output a JSON object like:
{"operations": [
  {"type": "track", "description": "Create a track named 'bass' with Serum VST", "parameters": {"name": "bass", "vst": "serum"}},
  {"type": "clip", "description": "Add a clip from bar 17 to 25", "parameters": {"start_bar": 17, "end_bar": 25}}
]}

## Examples
### Example 1 (English)
Prompt: Create a new audio track called 'Guitar' and add a 4-bar clip at bar 1.
Output:
{"operations": [
  {"type": "track", "description": "Create a new audio track called 'Guitar'", "parameters": {"name": "Guitar", "track_type": "audio"}},
  {"type": "clip", "description": "Add a 4-bar clip at bar 1 to 'Guitar'", "parameters": {"track_name": "Guitar", "start_bar": 1, "end_bar": 5}}
]}

### Example 2 (Spanish)
Prompt: Crea una nueva pista llamada 'Bajo' y ajusta el volumen a -6dB.
Output:
{"operations": [
  {"type": "track", "description": "Crear una nueva pista llamada 'Bajo'", "parameters": {"name": "Bajo"}},
  {"type": "volume", "description": "Ajustar el volumen de 'Bajo' a -6dB", "parameters": {"track_name": "Bajo", "volume": "-6dB"}}
]}

### Example 3 (French)
Prompt: Ajoute une piste MIDI nommée 'Synth' et ajoute un accord de Do majeur à la mesure 4.
Output:
{"operations": [
  {"type": "track", "description": "Ajouter une piste MIDI nommée 'Synth'", "parameters": {"name": "Synth", "track_type": "midi"}},
  {"type": "midi", "description": "Ajouter un accord de Do majeur à la mesure 4 sur 'Synth'", "parameters": {"track_name": "Synth", "chord": "C major", "bar": 4}}
]}

### Example 4 (German)
Prompt: Füge einen Reverb-Effekt zur 'Gitarre'-Spur hinzu und setze den Wet-Wert auf 50%.
Output:
{"operations": [
  {"type": "effect", "description": "Füge einen Reverb-Effekt zur 'Gitarre'-Spur hinzu", "parameters": {"track_name": "Gitarre", "effect": "reverb"}},
  {"type": "effect", "description": "Setze den Wet-Wert des Reverb-Effekts auf 50% auf 'Gitarre'", "parameters": {"track_name": "Gitarre", "effect": "reverb", "wet": 0.5}}
]}

### Example 5 (Japanese)
Prompt: トラック「ピアノ」に4小節のクリップを作成し、音量を-3dBに設定してください。
Output:
{"operations": [
  {"type": "clip", "description": "トラック「ピアノ」に4小節のクリップを作成", "parameters": {"track_name": "ピアノ", "start_bar": 1, "end_bar": 5}},
  {"type": "volume", "description": "トラック「ピアノ」の音量を-3dBに設定", "parameters": {"track_name": "ピアノ", "volume": "-3dB"}}
]}

### Example 6 (Complex, Multistep)
Prompt: Create a new track called 'Lead', set its volume to -3dB, and add reverb.
Output:
{"operations": [
  {"type": "track", "description": "Create a new track called 'Lead'", "parameters": {"name": "Lead"}},
  {"type": "volume", "description": "Set the volume of 'Lead' to -3dB", "parameters": {"track_name": "Lead", "volume": "-3dB"}},
  {"type": "effect", "description": "Add reverb to 'Lead'", "parameters": {"track_name": "Lead", "effect": "reverb"}}
]}

# End of static prompt
"""


class OperationIdentifier(BaseAgent):
    """Agent responsible for identifying operations in natural language prompts using LLM."""

    def __init__(self):
        super().__init__()
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    def can_handle(self, operation: str) -> bool:
        """This agent can handle any operation as it's the entry point."""
        return True

    def execute(
        self, prompt: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Parse the prompt and identify operations using LLM."""
        operations = self.identify_operations_with_llm(prompt)
        return {
            "operations": operations,
            "original_prompt": prompt,
            "context": context or {},
        }

    def identify_operations_with_llm(self, user_prompt: str) -> list[dict[str, Any]]:
        """Use LLM to identify operations in the prompt using optimized prompt structure and cache monitoring."""
        # Build the full prompt: static first, dynamic last
        full_prompt = OPTIMIZED_STATIC_PROMPT + "\n\n# USER REQUEST\n" + user_prompt
        try:
            response = self.client.responses.create(
                model="gpt-4o",
                instructions=full_prompt,
                input=user_prompt,
                user="magda-daw-api",
            )
            import json

            content = response.output_text
            # Cache monitoring (if available)
            usage = getattr(response, "usage", None)
            if usage is not None:
                cached = getattr(
                    getattr(usage, "prompt_tokens_details", None), "cached_tokens", 0
                )
                if cached > 0:
                    print(f"[MAGDA] Cache hit: {cached} tokens")
            if content is None:
                return []
            result = json.loads(content)
            operations = result.get("operations", [])
            return operations
        except Exception as e:
            print(f"Error identifying operations: {e}")
            return []

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can identify."""
        return ["track", "clip", "volume", "effect", "midi"]

    def extract_operation_chain(self, prompt: str) -> list[dict[str, Any]]:
        operations = self.identify_operations_with_llm(prompt)
        operation_chain = []
        current_context = {}
        for op in operations:
            if op["type"] == "track":
                track_info = op.get("parameters", {})
                current_context["track"] = track_info
                operation_chain.append(
                    {"operation": op, "context": current_context.copy()}
                )
            else:
                operation_chain.append(
                    {"operation": op, "context": current_context.copy()}
                )
        return operation_chain
