#!/usr/bin/env python3
"""
Test script to run the problematic MIDI prompt in isolation.
"""

import sys
from pathlib import Path

# Add the project root to the path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root / "python"))

from magda.pipeline import MAGDAPipeline


def test_midi_prompt_isolated():
    """Test the problematic MIDI prompt in isolation."""
    prompt = "Delete the note at beat 2.5 on 'Bass'"

    print("=== Testing MIDI prompt in isolation ===")
    print(f"Prompt: {prompt}")

    pipeline = MAGDAPipeline()

    try:
        print("Processing prompt...")
        result = pipeline.process_prompt(prompt)
        print(f"✅ Success: {result}")

        if result and "operations" in result and len(result["operations"]) > 0:
            print("✅ Operations identified successfully!")
            return True
        else:
            print("❌ No operations identified")
            return False

    except Exception as e:
        print(f"❌ Error: {e}")
        return False


if __name__ == "__main__":
    success = test_midi_prompt_isolated()
    if success:
        print("\n🎉 Test passed!")
    else:
        print("\n💥 Test failed!")
