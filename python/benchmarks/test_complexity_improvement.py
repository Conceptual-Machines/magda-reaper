#!/usr/bin/env python3
"""
Test script to demonstrate the improvement of semantic complexity detection.

This script compares the old word-count based approach with the new semantic
approach to show how much better the complexity detection has become.
"""

import os
import sys

# Add the magda package to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "."))

from magda.complexity import get_complexity_detector
from magda.utils import assess_task_complexity


def old_complexity_detection(prompt: str) -> str:
    """
    Simulate the old word-count based complexity detection.

    This is a simplified version of what the old approach would do.
    """
    if not prompt or not prompt.strip():
        return "simple"

    # Count words
    word_count = len(prompt.split())

    # Count potential operations (rough heuristic)
    operation_indicators = [
        "create",
        "add",
        "delete",
        "remove",
        "set",
        "change",
        "move",
        "crear",
        "agregar",
        "eliminar",
        "quitar",
        "establecer",
        "cambiar",
        "mover",
        "ajouter",
        "supprimer",
        "retirer",
        "définir",
        "changer",
        "déplacer",
        "hinzufügen",
        "löschen",
        "entfernen",
        "einstellen",
        "ändern",
        "verschieben",
        "作成",
        "追加",
        "削除",
        "設定",
        "変更",
        "移動",
    ]

    operation_count = sum(
        1 for indicator in operation_indicators if indicator.lower() in prompt.lower()
    )

    # Simple thresholds (old approach)
    if word_count <= 5 and operation_count <= 1:
        return "simple"
    elif word_count <= 10 and operation_count <= 2:
        return "medium"
    else:
        return "complex"


def test_complexity_improvement():
    """Test and compare old vs new complexity detection approaches."""

    print("=== Complexity Detection Improvement Test ===\n")

    # Test cases that show the improvement
    test_cases = [
        # Case 1: Short but complex command
        {
            "prompt": "Create track, add reverb, set volume",
            "expected": "complex",
            "description": "Short but multi-operation command",
        },
        # Case 2: Long but simple command
        {
            "prompt": "Please set the volume of the guitar track to exactly negative six decibels",
            "expected": "simple",
            "description": "Long but single operation",
        },
        # Case 3: Ambiguous command
        {
            "prompt": "Do something with the track",
            "expected": "simple",
            "description": "Vague command",
        },
        # Case 4: Complex conditional command
        {
            "prompt": "If the guitar track exists, add reverb with 30% wet, otherwise create it first",
            "expected": "complex",
            "description": "Conditional logic",
        },
        # Case 5: Multilingual simple command
        {
            "prompt": "Crea una nueva pista llamada 'Guitarra'",
            "expected": "simple",
            "description": "Spanish track creation",
        },
        # Case 6: Technical jargon
        {
            "prompt": "Apply 4:1 compression with -10dB threshold and 50ms attack",
            "expected": "medium",
            "description": "Technical effect parameters",
        },
        # Case 7: Invalid input
        {
            "prompt": "This is not a DAW command at all",
            "expected": "simple",
            "description": "Invalid input",
        },
        # Case 8: Empty input
        {"prompt": "", "expected": "simple", "description": "Empty input"},
    ]

    print("Comparing old vs new complexity detection:")
    print("=" * 100)
    print(
        f"{'#':<2} {'Description':<35} {'Old':<8} {'New':<8} {'Expected':<8} {'Improvement':<15}"
    )
    print("-" * 100)

    improvements = 0
    total_cases = len(test_cases)

    for i, case in enumerate(test_cases, 1):
        prompt = case["prompt"]
        expected = case["expected"]
        description = case["description"]

        # Get old complexity
        old_complexity = old_complexity_detection(prompt)

        # Get new complexity (with semantic detection)
        new_complexity = assess_task_complexity(prompt)

        # Determine if this is an improvement
        old_correct = old_complexity == expected
        new_correct = new_complexity == expected

        if new_correct and not old_correct:
            improvement = "✅ Better"
            improvements += 1
        elif old_correct and not new_correct:
            improvement = "❌ Worse"
        elif old_correct and new_correct:
            improvement = "✅ Same"
        else:
            improvement = "❌ Both wrong"

        print(
            f"{i:<2} {description:<35} {old_complexity:<8} {new_complexity:<8} {expected:<8} {improvement:<15}"
        )

    print("-" * 100)
    print(
        f"Improvement rate: {improvements}/{total_cases} cases ({improvements / total_cases * 100:.1f}%)"
    )

    print("\n" + "=" * 100)
    print("Detailed Analysis:")
    print("-" * 100)

    # Show detailed analysis for a few key cases
    key_cases = [0, 1, 3, 4]  # Most interesting cases

    for idx in key_cases:
        case = test_cases[idx]
        prompt = case["prompt"]
        expected = case["expected"]

        print(f"\nCase {idx + 1}: {case['description']}")
        print(f"Prompt: {repr(prompt)}")
        print(f"Expected: {expected}")

        # Old approach
        old_complexity = old_complexity_detection(prompt)
        print(f"Old approach: {old_complexity} (word count: {len(prompt.split())})")

        # New approach with semantic detector
        detector = get_complexity_detector()
        result = detector.detect_complexity(prompt)
        print(
            f"New approach: {result.complexity} (confidence: {result.confidence:.3f})"
        )
        if result.operation_type:
            print(
                f"  Operation type: {result.operation_type} (confidence: {result.operation_confidence:.3f})"
            )

        # Show similar prompts if available
        if hasattr(detector, "get_similar_prompts"):
            similar = detector.get_similar_prompts(prompt, top_k=2)
            if similar:
                print("  Similar examples:")
                for ex_prompt, category, similarity in similar:
                    print(f"    - {ex_prompt} ({similarity:.3f})")

    print("\n" + "=" * 100)
    print("Summary:")
    print("-" * 100)
    print("✅ Semantic complexity detection provides:")
    print("   • More accurate classification based on meaning, not just word count")
    print("   • Better handling of multilingual prompts")
    print("   • Operation type classification")
    print("   • Confidence scores for decisions")
    print("   • Graceful fallback to simple detection when needed")
    print(
        "\n🎯 This leads to better model selection and improved pipeline performance!"
    )


if __name__ == "__main__":
    test_complexity_improvement()
