#!/usr/bin/env python3
"""
Complexity Detection and Model Selection Benchmark

This script benchmarks different approaches:
1. Algorithmic complexity detection (word-count based)
2. Semantic complexity detection (sentence-transformers)
3. Hardcoded model selection (gpt-4.1, gpt-4o, o3-mini)

Tests the same prompts across all approaches to compare performance.
"""

import os
import sys
import time
from statistics import mean, median
from typing import Any

# Add the magda package to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "."))

from magda.complexity import get_complexity_detector
from magda.utils import (
    assess_task_complexity,
    requires_reasoning,
    select_model_for_task,
)


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


def benchmark_prompts() -> list[dict[str, Any]]:
    """Define the benchmark prompts with expected classifications."""
    return [
        {
            "prompt": "set volume to -6db",
            "expected_complexity": "simple",
            "expected_operation": "volume",
            "description": "Simple volume command",
        },
        {
            "prompt": "create new track called guitar",
            "expected_complexity": "simple",
            "expected_operation": "track",
            "description": "Simple track creation",
        },
        {
            "prompt": "mute drums",
            "expected_complexity": "simple",
            "expected_operation": "volume",
            "description": "Simple mute command",
        },
        {
            "prompt": "create track and add reverb",
            "expected_complexity": "medium",
            "expected_operation": "effect",
            "description": "Two operations: track + effect",
        },
        {
            "prompt": "mute drums and set bass volume -3db",
            "expected_complexity": "medium",
            "expected_operation": "volume",
            "description": "Two volume operations",
        },
        {
            "prompt": "add compression to guitar track",
            "expected_complexity": "medium",
            "expected_operation": "effect",
            "description": "Effect with target specification",
        },
        {
            "prompt": "create track add reverb set volume -3db export mix",
            "expected_complexity": "complex",
            "expected_operation": "effect",
            "description": "Multiple operations",
        },
        {
            "prompt": "for all tracks except drums add compressor threshold -10db",
            "expected_complexity": "complex",
            "expected_operation": "effect",
            "description": "Conditional logic with multiple operations",
        },
        {
            "prompt": "create bass track with serum add compression 4:1 ratio set volume -6db pan left",
            "expected_complexity": "complex",
            "expected_operation": "volume",
            "description": "Complex multi-step with parameters",
        },
        {
            "prompt": "if guitar track exists add reverb 30% wet otherwise create it first",
            "expected_complexity": "complex",
            "expected_operation": "effect",
            "description": "Conditional logic",
        },
        {
            "prompt": "create a track called bass and add a  8  bar clip starting from bar 17",
            "expected_complexity": "medium",
            "expected_operation": "clip",
            "description": "Track + clip with specific parameters",
        },
        {
            "prompt": "crea una nueva pista llamada guitarra",
            "expected_complexity": "simple",
            "expected_operation": "track",
            "description": "Spanish track creation",
        },
        {
            "prompt": "ajoute une nouvelle piste nommée guitare",
            "expected_complexity": "simple",
            "expected_operation": "track",
            "description": "French track creation",
        },
        {
            "prompt": "新しいトラック「ギター」を作成してください",
            "expected_complexity": "simple",
            "expected_operation": "track",
            "description": "Japanese track creation",
        },
        {
            "prompt": "this is not a valid daw command",
            "expected_complexity": "simple",
            "expected_operation": "unknown",
            "description": "Invalid input",
        },
        {
            "prompt": "random text that doesn't make sense",
            "expected_complexity": "simple",
            "expected_operation": "unknown",
            "description": "Nonsense input",
        },
    ]


def run_algorithmic_benchmark(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run benchmark using algorithmic complexity detection."""
    print("🔧 Running Algorithmic Complexity Detection Benchmark")
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
        complexity = old_complexity_detection(prompt)

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


def run_semantic_benchmark(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run benchmark using semantic complexity detection."""
    print("\n🧠 Running Semantic Complexity Detection Benchmark")
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


def run_hardcoded_benchmark(prompts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Run benchmark using hardcoded models."""
    print("\n🔒 Running Hardcoded Model Benchmark")
    print("=" * 80)

    hardcoded_models = [
        ("gpt-4.1", "gpt-4.1"),
        ("gpt-4o", "gpt-4o"),
        ("o3-mini", "o3-mini"),
    ]

    results = []

    for model_name, model_id in hardcoded_models:
        print(f"\n📊 Testing with {model_name}:")
        print("-" * 60)

        model_results = []

        for i, test_case in enumerate(prompts, 1):
            prompt = test_case["prompt"]
            expected = test_case["expected_complexity"]

            print(f"  {i:2d}. {test_case['description']}")
            print(f"      Prompt: {repr(prompt)}")

            # Measure latency for hardcoded approach
            start_time = time.perf_counter()

            # Use semantic complexity detection for consistency
            complexity = assess_task_complexity(prompt)

            end_time = time.perf_counter()
            latency_ms = (end_time - start_time) * 1000

            result = {
                "approach": f"hardcoded_{model_name}",
                "test_case": test_case,
                "model_name": model_name,
                "model_id": model_id,
                "complexity": complexity,
                "expected_complexity": expected,
                "complexity_correct": complexity == expected,
                "latency_ms": latency_ms,
            }

            print(
                f"      → Complexity: {complexity} (expected: {expected}) {'✅' if complexity == expected else '❌'}"
            )
            print(f"      → Model: {model_id}")
            print(f"      → Latency: {latency_ms:.2f}ms")

            model_results.append(result)

        results.extend(model_results)

    return results


def calculate_metrics(
    results: list[dict[str, Any]], approach_name: str
) -> dict[str, Any]:
    """Calculate performance metrics for an approach."""
    total = len(results)
    complexity_correct = sum(1 for r in results if r.get("complexity_correct", False))

    # Count model usage
    model_usage = {}
    for result in results:
        orchestrator = result.get("orchestrator_model", "unknown")
        specialized = result.get("specialized_model", "unknown")

        model_usage[orchestrator] = model_usage.get(orchestrator, 0) + 1
        model_usage[specialized] = model_usage.get(specialized, 0) + 1

    # Count reasoning detection
    reasoning_required = sum(1 for r in results if r.get("needs_reasoning", False))

    # Calculate latency statistics
    latencies = [r.get("latency_ms", 0) for r in results]
    latency_stats = {
        "mean": mean(latencies) if latencies else 0,
        "median": median(latencies) if latencies else 0,
        "min": min(latencies) if latencies else 0,
        "max": max(latencies) if latencies else 0,
        "total": sum(latencies) if latencies else 0,
    }

    metrics = {
        "approach": approach_name,
        "total_tests": total,
        "complexity_accuracy": complexity_correct / total if total > 0 else 0,
        "complexity_correct": complexity_correct,
        "model_usage": model_usage,
        "reasoning_required": reasoning_required,
        "reasoning_percentage": reasoning_required / total if total > 0 else 0,
        "latency_stats": latency_stats,
    }

    return metrics


def print_summary(
    algorithmic_results: list[dict[str, Any]],
    semantic_results: list[dict[str, Any]],
    hardcoded_results: list[dict[str, Any]],
):
    """Print comprehensive benchmark summary."""
    print("\n" + "=" * 100)
    print("📊 BENCHMARK SUMMARY")
    print("=" * 100)

    # Calculate metrics
    algorithmic_metrics = calculate_metrics(algorithmic_results, "Algorithmic")
    semantic_metrics = calculate_metrics(semantic_results, "Semantic")

    # For hardcoded, group by model
    hardcoded_by_model = {}
    for result in hardcoded_results:
        model_name = result["model_name"]
        if model_name not in hardcoded_by_model:
            hardcoded_by_model[model_name] = []
        hardcoded_by_model[model_name].append(result)

    hardcoded_metrics = {}
    for model_name, model_results in hardcoded_by_model.items():
        hardcoded_metrics[model_name] = calculate_metrics(
            model_results, f"Hardcoded {model_name}"
        )

    # Print comparison table
    print("\nComplexity Detection Accuracy:")
    print("-" * 60)
    print(
        f"{'Approach':<20} {'Accuracy':<10} {'Correct/Total':<15} {'Reasoning %':<12}"
    )
    print("-" * 60)

    print(
        f"{algorithmic_metrics['approach']:<20} {algorithmic_metrics['complexity_accuracy']:.1%} {algorithmic_metrics['complexity_correct']}/{algorithmic_metrics['total_tests']:<15} {algorithmic_metrics['reasoning_percentage']:.1%}"
    )
    print(
        f"{semantic_metrics['approach']:<20} {semantic_metrics['complexity_accuracy']:.1%} {semantic_metrics['complexity_correct']}/{semantic_metrics['total_tests']:<15} {semantic_metrics['reasoning_percentage']:.1%}"
    )

    for model_name, metrics in hardcoded_metrics.items():
        print(
            f"{metrics['approach']:<20} {metrics['complexity_accuracy']:.1%} {metrics['complexity_correct']}/{metrics['total_tests']:<15} {metrics['reasoning_percentage']:.1%}"
        )

    # Print model usage
    print("\nModel Usage Distribution:")
    print("-" * 60)

    print("Algorithmic Approach:")
    for model, count in algorithmic_metrics["model_usage"].items():
        print(f"  {model}: {count}")

    print("\nSemantic Approach:")
    for model, count in semantic_metrics["model_usage"].items():
        print(f"  {model}: {count}")

    # Print detailed analysis
    print("\nDetailed Analysis:")
    print("-" * 60)

    # Find cases where approaches differ
    differences = []
    for i, (algo, sem) in enumerate(
        zip(algorithmic_results, semantic_results, strict=False)
    ):
        if algo["complexity"] != sem["complexity"]:
            differences.append(
                {
                    "index": i + 1,
                    "prompt": algo["test_case"]["prompt"],
                    "algorithmic": algo["complexity"],
                    "semantic": sem["complexity"],
                    "expected": algo["expected_complexity"],
                }
            )

    if differences:
        print(f"\nCases where approaches differ ({len(differences)}):")
        for diff in differences:
            print(f"  {diff['index']}. {repr(diff['prompt'])}")
            print(
                f"     Algorithmic: {diff['algorithmic']}, Semantic: {diff['semantic']}, Expected: {diff['expected']}"
            )
    else:
        print("\nAll approaches agree on complexity classification!")

    # Recommendations
    print("\nRecommendations:")
    print("-" * 60)

    if (
        semantic_metrics["complexity_accuracy"]
        > algorithmic_metrics["complexity_accuracy"]
    ):
        print("✅ Semantic approach shows better accuracy")
    else:
        print("⚠️  Algorithmic approach shows better accuracy")

    if semantic_metrics["reasoning_percentage"] > 0:
        print(
            f"✅ Semantic approach detected {semantic_metrics['reasoning_required']} prompts requiring reasoning"
        )

    # Cost analysis
    total_cost_algorithmic = sum(
        1 for r in algorithmic_results if "o3" in r.get("orchestrator_model", "")
    )
    total_cost_semantic = sum(
        1 for r in semantic_results if "o3" in r.get("orchestrator_model", "")
    )

    print("\nCost Analysis (o3-mini usage):")
    print(f"  Algorithmic: {total_cost_algorithmic}/{len(algorithmic_results)} prompts")
    print(f"  Semantic: {total_cost_semantic}/{len(semantic_results)} prompts")

    if total_cost_semantic < total_cost_algorithmic:
        print("✅ Semantic approach is more cost-effective")
    else:
        print("⚠️  Algorithmic approach is more cost-effective")


def save_benchmark_data(all_results: list[dict[str, Any]], filename: str = None):
    """Save benchmark data to JSON file."""
    import json
    from datetime import datetime

    if filename is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"benchmark_results_{timestamp}.json"

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

    print(f"\n💾 Benchmark data saved to: {filename}")
    return filename


def main():
    """Run the complete benchmark and collect data."""
    print("🚀 MAGDA Complexity Detection and Model Selection Benchmark")
    print("=" * 100)

    # Get benchmark prompts
    prompts = benchmark_prompts()
    print(f"Testing {len(prompts)} prompts across different approaches...")

    # Run benchmarks and collect all data
    print("\n📊 Collecting benchmark data...")
    algorithmic_results = run_algorithmic_benchmark(prompts)
    semantic_results = run_semantic_benchmark(prompts)
    hardcoded_results = run_hardcoded_benchmark(prompts)

    # Combine all results
    all_results = algorithmic_results + semantic_results + hardcoded_results

    # Save raw data
    data_file = save_benchmark_data(all_results)

    print("\n" + "=" * 100)
    print("✅ Data collection completed!")
    print(f"📁 Raw data saved to: {data_file}")
    print("🔍 Run analysis with: python analyze_benchmark.py <data_file>")


if __name__ == "__main__":
    main()
