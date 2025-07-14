#!/usr/bin/env python3
"""
Test script to verify semantic complexity detection integration.

This script tests that the new semantic complexity detector is properly
integrated into the MAGDA pipeline and works with the existing utils.
"""

import os
import sys

# Add the magda package to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "."))

from magda.complexity import get_complexity_detector
from magda.utils import assess_task_complexity, select_model_for_task


def test_semantic_integration():
    """Test that semantic complexity detection is properly integrated."""

    print("=== Testing Semantic Complexity Detection Integration ===\n")

    # Test prompts of varying complexity
    test_prompts = [
        # Simple prompts
        "Set the volume to -6dB",
        "Create a new track called 'Guitar'",
        "Mute the drums",
        # Medium prompts
        "Create a new track and add reverb",
        "Mute the drums and set the bass volume to -3dB",
        "Add compression to the guitar track",
        # Complex prompts
        "Create a new track, add reverb, set the volume to -3dB, and export the mix",
        "For all tracks except drums, add a compressor and set the threshold to -10dB",
        "Create a bass track with Serum, add compression with 4:1 ratio, set volume to -6dB, and pan left",
        # Multilingual prompts
        "Crea una nueva pista llamada 'Guitarra'",
        "Ajoute une nouvelle piste nommée 'Guitare'",
        "新しいトラック「ギター」を作成してください",
        # Edge cases
        "This is not a valid DAW command",
        "Random text that doesn't make sense",
        "",
        "123456789",
        # New test: multi-step, real-world, slightly sloppy
        "create a track called bass and add a  8  bar clip starting from bar 17",
    ]

    print("Testing complexity detection for various prompts:")
    print("-" * 80)

    for i, prompt in enumerate(test_prompts, 1):
        print(f"\n{i:2d}. Prompt: {repr(prompt)}")

        # Test the integrated function
        complexity = assess_task_complexity(prompt)

        # Test model selection
        orchestrator_model = select_model_for_task("orchestrator", complexity, prompt)
        specialized_model = select_model_for_task("specialized", complexity, prompt)

        print(f"    → Complexity: {complexity}")
        print(f"    → Orchestrator model: {orchestrator_model}")
        print(f"    → Specialized model: {specialized_model}")

    print("\n" + "=" * 80)
    print("Testing direct semantic detector access:")
    print("-" * 80)

    # Test direct access to the semantic detector
    try:
        detector = get_complexity_detector()
        print(f"Detector type: {type(detector).__name__}")

        if hasattr(detector, "is_multilingual_supported"):
            print(f"Multilingual support: {detector.is_multilingual_supported()}")

        # Test a few prompts directly
        test_prompt = "Create a new track and add reverb with 30% wet"
        result = detector.detect_complexity(test_prompt)
        print(f"\nDirect test - Prompt: {repr(test_prompt)}")
        print(f"  → Complexity: {result.complexity}")
        print(f"  → Confidence: {result.confidence:.3f}")
        print(f"  → Operation type: {result.operation_type}")
        if result.operation_confidence is not None:
            print(f"  → Operation confidence: {result.operation_confidence:.3f}")
        else:
            print("  → Operation confidence: N/A")

        # Test similar prompts if available
        if hasattr(detector, "get_similar_prompts"):
            similar = detector.get_similar_prompts(test_prompt, top_k=3)
            if similar:
                print("  → Similar prompts:")
                for prompt, category, similarity in similar:
                    print(f"    - {prompt} ({category}, {similarity:.3f})")

    except Exception as e:
        print(f"Error testing direct detector: {e}")

    print("\n" + "=" * 80)
    print("Integration test completed!")


if __name__ == "__main__":
    test_semantic_integration()
