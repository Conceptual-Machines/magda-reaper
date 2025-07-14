#!/usr/bin/env python3
"""
Test script to run simpler MIDI prompts to isolate the issue.
"""

import sys
from pathlib import Path

# Add the project root to the path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root / "python"))

from magda.pipeline import MAGDAPipeline


def test_midi_prompts():
    """Test various MIDI prompts to isolate the issue."""
    prompts = [
        "Add a C note to the Piano track",
        "Delete a note from the Bass track",
        "Delete the note at beat 2.5 on 'Bass'",  # The problematic one
        "Add a C major chord at bar 1 on 'Piano'",  # This one worked in the full test
    ]

    pipeline = MAGDAPipeline()

    for i, prompt in enumerate(prompts, 1):
        print(f"\n=== Test {i}: {prompt} ===")

        try:
            result = pipeline.process_prompt(prompt)

            if result and "operations" in result and len(result["operations"]) > 0:
                print(f"✅ SUCCESS: {len(result['operations'])} operations identified")
                for op in result["operations"]:
                    print(
                        f"   - {op.get('type', 'unknown')}: {op.get('description', 'no description')}"
                    )
            else:
                print("❌ FAILED: No operations identified")

        except Exception as e:
            print(f"❌ ERROR: {e}")


if __name__ == "__main__":
    test_midi_prompts()
