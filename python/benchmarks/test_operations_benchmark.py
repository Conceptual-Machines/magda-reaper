#!/usr/bin/env python3
"""
Operations Benchmark for MAGDA

Tests actual DAW operations using the same complexity detection matrix
to evaluate real-world performance of different approaches.
"""

import json
import os
import time
from datetime import datetime
from typing import Any

# Set offline mode BEFORE importing anything
os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"
os.environ["HF_DATASETS_OFFLINE"] = "1"
os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"

from magda.complexity.semantic_detector import get_complexity_detector
from magda.pipeline import MAGDAPipeline
from magda.utils import (
    assess_task_complexity,
    requires_reasoning,
    select_model_for_task,
)


def get_operation_prompts() -> list[dict[str, Any]]:
    """Get realistic DAW operation prompts for testing."""
    return [
        # Simple Operations
        {
            "description": "Simple volume command",
            "prompt": "set volume to -6db",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Simple track creation",
            "prompt": "create new track called guitar",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Simple mute command",
            "prompt": "mute drums",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Simple effect addition",
            "prompt": "add reverb to bass track",
            "expected_complexity": "simple",
            "expected_operation": "effect",
        },
        # Medium Complexity Operations
        {
            "description": "Track + effect combination",
            "prompt": "create track and add reverb",
            "expected_complexity": "medium",
            "expected_operation": "track",
        },
        {
            "description": "Multiple volume operations",
            "prompt": "mute drums and set bass volume -3db",
            "expected_complexity": "medium",
            "expected_operation": "volume",
        },
        {
            "description": "Effect with parameters",
            "prompt": "add compression to guitar track",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Clip creation",
            "prompt": "create 4 bar clip on drums track",
            "expected_complexity": "medium",
            "expected_operation": "clip",
        },
        # Complex Operations
        {
            "description": "Multi-step operation",
            "prompt": "create track add reverb set volume -3db export mix",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "Conditional logic",
            "prompt": "if guitar track exists add reverb 30% wet otherwise create it first",
            "expected_complexity": "complex",
            "expected_operation": "effect",
        },
        {
            "description": "Complex multi-step with parameters",
            "prompt": "create bass track with serum add compression 4:1 ratio set volume -6db pan left",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "Track + clip with specific parameters",
            "prompt": "create a track called bass and add a 8 bar clip starting from bar 17",
            "expected_complexity": "medium",
            "expected_operation": "track",
        },
        # Multilingual Operations
        {
            "description": "Spanish track creation",
            "prompt": "crea una nueva pista llamada guitarra",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "French track creation",
            "prompt": "ajoute une nouvelle piste nommée guitare",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Japanese track creation",
            "prompt": "新しいトラック「ギター」を作成してください",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        # Edge Cases
        {
            "description": "Invalid input",
            "prompt": "this is not a valid daw command",
            "expected_complexity": "simple",
            "expected_operation": "unknown",
        },
        {
            "description": "Nonsense input",
            "prompt": "random text that doesn't make sense",
            "expected_complexity": "simple",
            "expected_operation": "unknown",
        },
        # Realistic Typos and Sloppy Input
        {
            "description": "Typo in track name",
            "prompt": "create new trak called gutar",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo in effect name",
            "prompt": "add reberb to bass",
            "expected_complexity": "simple",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in volume command",
            "prompt": "set volme to -6db",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Mixed case and typos",
            "prompt": "ADD COMPRESSHUN to GUITAR trak",
            "expected_complexity": "simple",
            "expected_operation": "effect",
        },
        {
            "description": "Missing spaces",
            "prompt": "createtrackcalledbass",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Extra spaces and typos",
            "prompt": "add    reberb    to    gutar    trak",
            "expected_complexity": "simple",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in Spanish",
            "prompt": "crea una nueva pista llamada gutarra",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo in French",
            "prompt": "ajoute une nouvelle piste nommée gutare",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo in Japanese",
            "prompt": "新しいトラック「ギタ」を作成してください",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Sloppy capitalization",
            "prompt": "CrEaTe NeW tRaCk CaLlEd BaSs",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo in complex prompt",
            "prompt": "create bass trak with serum add compreshun 4:1 ratio set volme -6db",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "Multiple typos in one prompt",
            "prompt": "add reberb to gutar trak and set volme to -3db",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in conditional logic",
            "prompt": "if gutar trak exists add reberb 30% wet otherwise create it first",
            "expected_complexity": "complex",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in clip creation",
            "prompt": "create 4 bar clp on drums trak",
            "expected_complexity": "medium",
            "expected_operation": "clip",
        },
        {
            "description": "Typo in mute command",
            "prompt": "mute drms",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Typo in compression",
            "prompt": "add compreshun to gutar trak",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in track creation with VST",
            "prompt": "create bass trak with seram",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo in volume with dB",
            "prompt": "set volme to -6db on bass trak",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Typo in reverb parameters",
            "prompt": "add reberb with 30% wet mix to gutar",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in multi-step operation",
            "prompt": "create trak add reberb set volme -3db",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        # VST Plugin and Instrument Context
        {
            "description": "Create track with Serum",
            "prompt": "create bass track with serum",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Create track with Addictive Drums",
            "prompt": "create drums track with addictive drums",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Create track with Kontakt",
            "prompt": "create piano track with kontakt",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Add effect to specific track",
            "prompt": "add compression to bass track",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Set volume on specific track",
            "prompt": "set volume to -6db on drums track",
            "expected_complexity": "simple",
            "expected_operation": "volume",
        },
        {
            "description": "Complex with VST and effects",
            "prompt": "create bass track with serum add compression 4:1 ratio set volume -6db",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "Typo with VST name",
            "prompt": "create bass trak with seram",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "Typo with instrument name",
            "prompt": "add compreshun to gutar trak",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Multiple VSTs mentioned",
            "prompt": "create track with serum and add addictive drums",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "VST with parameters",
            "prompt": "create bass track with serum and set attack to 0.01",
            "expected_complexity": "medium",
            "expected_operation": "track",
        },
        {
            "description": "Instrument-specific effect",
            "prompt": "add reverb to piano track",
            "expected_complexity": "medium",
            "expected_operation": "effect",
        },
        {
            "description": "Typo in VST name",
            "prompt": "create drums trak with addictiv drums",
            "expected_complexity": "simple",
            "expected_operation": "track",
        },
        {
            "description": "VST with complex parameters",
            "prompt": "create bass track with serum set cutoff to 1000hz resonance to 0.5",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
        {
            "description": "Multiple instruments",
            "prompt": "create guitar and bass tracks",
            "expected_complexity": "medium",
            "expected_operation": "track",
        },
        {
            "description": "VST with effect chain",
            "prompt": "create bass track with serum add compression and reverb",
            "expected_complexity": "complex",
            "expected_operation": "track",
        },
    ]


def run_algorithmic_operations(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run operations using algorithmic complexity detection."""
    print("🔧 Running Algorithmic Operations Benchmark")
    print("=" * 80)

    results = []

    for i, test_case in enumerate(prompts, 1):
        prompt = test_case["prompt"]
        expected = test_case["expected_complexity"]

        print(f"\n{i:2d}. {test_case['description']}")
        print(f"    Prompt: {repr(prompt)}")

        # Measure latency for algorithmic approach
        start_time = time.perf_counter()

        # Use old algorithmic approach
        complexity = assess_task_complexity(prompt)

        # Model selection (without reasoning detection)
        orchestrator_model = select_model_for_task("orchestrator", complexity, "")
        specialized_model = select_model_for_task("specialized", complexity, "")

        # Check reasoning manually
        needs_reasoning = requires_reasoning(prompt)

        end_time = time.perf_counter()
        latency_ms = (end_time - start_time) * 1000

        result = {
            "approach": "algorithmic",
            "test_case": test_case,
            "complexity": complexity,
            "orchestrator_model": orchestrator_model,
            "specialized_model": specialized_model,
            "needs_reasoning": needs_reasoning,
            "expected_complexity": expected,
            "complexity_correct": complexity == expected,
            "latency_ms": latency_ms,
        }

        print(
            f"    → Complexity: {complexity} (expected: {expected}) {'✅' if complexity == expected else '❌'}"
        )
        print(f"    → Orchestrator: {orchestrator_model}")
        print(f"    → Specialized: {specialized_model}")
        print(f"    → Reasoning: {'Required' if needs_reasoning else 'Not required'}")
        print(f"    → Latency: {latency_ms:.2f}ms")

        results.append(result)

    return results


def run_semantic_operations(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run operations using semantic complexity detection."""
    print("\n🧠 Running Semantic Operations Benchmark")
    print("=" * 80)

    results = []

    for i, test_case in enumerate(prompts, 1):
        prompt = test_case["prompt"]
        expected = test_case["expected_complexity"]

        print(f"\n{i:2d}. {test_case['description']}")
        print(f"    Prompt: {repr(prompt)}")

        # Measure latency for semantic approach
        start_time = time.perf_counter()

        # Use semantic approach
        complexity = assess_task_complexity(prompt)

        # Get semantic detector for additional info
        detector = get_complexity_detector()
        if hasattr(detector, "detect_complexity"):
            result = detector.detect_complexity(prompt)
            operation_type = result.operation_type
            confidence = result.confidence
        else:
            operation_type = "unknown"
            confidence = 0.0

        # Model selection (with reasoning detection)
        orchestrator_model = select_model_for_task("orchestrator", complexity, prompt)
        specialized_model = select_model_for_task("specialized", complexity, prompt)

        # Check reasoning manually
        needs_reasoning = requires_reasoning(prompt)

        end_time = time.perf_counter()
        latency_ms = (end_time - start_time) * 1000

        result = {
            "approach": "semantic",
            "test_case": test_case,
            "complexity": complexity,
            "operation_type": operation_type,
            "confidence": confidence,
            "orchestrator_model": orchestrator_model,
            "specialized_model": specialized_model,
            "needs_reasoning": needs_reasoning,
            "expected_complexity": expected,
            "complexity_correct": complexity == expected,
            "latency_ms": latency_ms,
        }

        print(
            f"    → Complexity: {complexity} (confidence: {confidence:.3f}) (expected: {expected}) {'✅' if complexity == expected else '❌'}"
        )
        print(f"    → Operation: {operation_type}")
        print(f"    → Orchestrator: {orchestrator_model}")
        print(f"    → Specialized: {specialized_model}")
        print(f"    → Reasoning: {'Required' if needs_reasoning else 'Not required'}")
        print(f"    → Latency: {latency_ms:.2f}ms")

        results.append(result)

    return results


def run_pipeline_operations(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run operations through the full MAGDA pipeline."""
    print("\n🚀 Running Full Pipeline Operations Benchmark")
    print("=" * 80)

    results = []
    pipeline = MAGDAPipeline()

    for i, test_case in enumerate(prompts, 1):
        prompt = test_case["prompt"]
        expected = test_case["expected_complexity"]

        print(f"\n{i:2d}. {test_case['description']}")
        print(f"    Prompt: {repr(prompt)}")

        # Measure latency for full pipeline
        start_time = time.perf_counter()

        try:
            # Run through full pipeline
            pipeline_result = pipeline.process_prompt(prompt)

            # Extract results
            daw_commands = pipeline_result.get("daw_commands", [])
            success = len(daw_commands) > 0

            end_time = time.perf_counter()
            latency_ms = (end_time - start_time) * 1000

            result = {
                "approach": "pipeline",
                "test_case": test_case,
                "success": success,
                "daw_commands": daw_commands,
                "command_count": len(daw_commands),
                "expected_complexity": expected,
                "latency_ms": latency_ms,
            }

            print(f"    → Success: {'✅' if success else '❌'}")
            print(f"    → Commands: {len(daw_commands)}")
            if daw_commands:
                for j, cmd in enumerate(daw_commands[:3], 1):  # Show first 3 commands
                    print(f"      {j}. {cmd}")
                if len(daw_commands) > 3:
                    print(f"      ... and {len(daw_commands) - 3} more")
            print(f"    → Latency: {latency_ms:.2f}ms")

        except Exception as e:
            end_time = time.perf_counter()
            latency_ms = (end_time - start_time) * 1000

            result = {
                "approach": "pipeline",
                "test_case": test_case,
                "success": False,
                "error": str(e),
                "daw_commands": [],
                "command_count": 0,
                "expected_complexity": expected,
                "latency_ms": latency_ms,
            }

            print(f"    → Success: ❌ (Error: {e})")
            print(f"    → Latency: {latency_ms:.2f}ms")

        results.append(result)

    return results


def save_operations_data(all_results: list[dict[str, Any]], filename: str = None):
    """Save operations benchmark data to JSON file."""
    if filename is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"operations_benchmark_results_{timestamp}.json"

    # Prepare data for saving
    data = {
        "metadata": {
            "timestamp": datetime.now().isoformat(),
            "total_results": len(all_results),
            "approaches": list(set(r["approach"] for r in all_results)),
        },
        "results": all_results,
    }

    with open(filename, "w") as f:
        json.dump(data, f, indent=2)

    print(f"\n💾 Operations benchmark data saved to: {filename}")
    return filename


def main():
    """Run the operations benchmark."""
    print("🚀 MAGDA Operations Benchmark")
    print("=" * 100)

    # Get operation prompts
    prompts = get_operation_prompts()
    print(f"Testing {len(prompts)} operations across different approaches...")

    # Run benchmarks and collect all data
    print("\n📊 Collecting operations benchmark data...")
    algorithmic_results = run_algorithmic_operations(prompts)
    semantic_results = run_semantic_operations(prompts)
    pipeline_results = run_pipeline_operations(prompts)

    # Combine all results
    all_results = algorithmic_results + semantic_results + pipeline_results

    # Save raw data
    data_file = save_operations_data(all_results)

    print("\n" + "=" * 100)
    print("✅ Operations benchmark completed!")
    print(f"📁 Raw data saved to: {data_file}")
    print("🔍 Run analysis with: python analyze_operations.py <data_file>")


if __name__ == "__main__":
    main()
