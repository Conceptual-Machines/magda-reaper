#!/usr/bin/env python3
"""
Test script for improved fast-path routing with fuzzy matching.
"""

from magda.utils import fast_path_route, requires_reasoning

# Test cases with various prompts
test_prompts = [
    # Effect operations
    "add compression to guitar track",
    "add reverb to bass",
    "put chorus on the vocals",
    "apply eq to drums",
    "add delay effect",
    "put distortion on guitar",
    # Track operations
    "create new track called guitar",
    "add a track for drums",
    "make a bass track",
    "create channel for vocals",
    # Volume operations
    "set volume to -6db",
    "turn up the bass",
    "mute drums",
    "adjust gain on guitar",
    "silence the track",
    # Edge cases
    "add compression and reverb",  # Should be ambiguous
    "create track and add effect",  # Should be ambiguous
    "set volume and mute",  # Should be ambiguous
]

print("🔧 Testing Fast-Path Routing with Fuzzy Matching")
print("=" * 60)

for prompt in test_prompts:
    result = fast_path_route(prompt)
    reasoning = requires_reasoning(prompt)

    status = "✅" if result else "❌"
    print(f"\n{status} {repr(prompt)}")
    print(f"   Fast-path: {result or 'None'}")
    print(f"   Reasoning: {'Required' if reasoning else 'Not required'}")

print("\n" + "=" * 60)
print("✅ Fast-path routing test completed!")
