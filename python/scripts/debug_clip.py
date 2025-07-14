#!/usr/bin/env python3
"""
Debug script to test clip operations that are hanging.
"""

import sys
from pathlib import Path

# Add the project root to the path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root / "python"))

from magda.pipeline import MAGDAPipeline


def test_clip_operation(prompt: str):
    """Test a single clip operation."""
    print(f"\n=== Testing: {prompt} ===")

    pipeline = MAGDAPipeline()

    try:
        print("Processing prompt...")
        result = pipeline.process_prompt(prompt)
        print(f"✅ Success: {result}")
        return True
    except Exception as e:
        print(f"❌ Error: {e}")
        return False


if __name__ == "__main__":
    # Test the problematic clip operations
    clip_prompts = [
        "Create a 4-bar clip on track 'Guitar'",
        "Delete the clip at bar 8 on 'Bass'",
        "Move the clip from bar 4 to bar 12 on 'Drums'",
    ]

    for prompt in clip_prompts:
        success = test_clip_operation(prompt)
        if not success:
            print(f"Failed on: {prompt}")
            break
