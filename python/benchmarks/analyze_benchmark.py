#!/usr/bin/env python3
"""
Benchmark Analysis Script

Loads benchmark data and performs comprehensive analysis including:
- Accuracy comparison
- Latency analysis
- Model usage patterns
- Cost analysis
- Detailed breakdowns
"""

import argparse
import json
from collections import defaultdict
from statistics import mean, median
from typing import Any


def load_benchmark_data(filename: str) -> dict[str, Any]:
    """Load benchmark data from JSON file."""
    with open(filename) as f:
        data = json.load(f)
    return data


def group_results_by_approach(
    results: list[dict[str, Any]],
) -> dict[str, list[dict[str, Any]]]:
    """Group results by approach type."""
    grouped = defaultdict(list)
    for result in results:
        approach = result["approach"]
        grouped[approach].append(result)
    return dict(grouped)


def calculate_approach_metrics(
    results: list[dict[str, Any]], approach_name: str
) -> dict[str, Any]:
    """Calculate comprehensive metrics for an approach."""
    total = len(results)
    complexity_correct = sum(1 for r in results if r.get("complexity_correct", False))

    # Model usage analysis
    model_usage = defaultdict(int)
    for result in results:
        orchestrator = result.get("orchestrator_model", "unknown")
        specialized = result.get("specialized_model", "unknown")
        model_usage[orchestrator] += 1
        model_usage[specialized] += 1

    # Reasoning detection
    reasoning_required = sum(1 for r in results if r.get("needs_reasoning", False))

    # Latency analysis
    latencies = [r.get("latency_ms", 0) for r in results]
    latency_stats = {
        "mean": mean(latencies) if latencies else 0,
        "median": median(latencies) if latencies else 0,
        "min": min(latencies) if latencies else 0,
        "max": max(latencies) if latencies else 0,
        "total": sum(latencies) if latencies else 0,
    }

    # Cost analysis (o3-mini usage)
    o3_usage = sum(1 for r in results if "o3" in r.get("orchestrator_model", ""))

    return {
        "approach": approach_name,
        "total_tests": total,
        "complexity_accuracy": complexity_correct / total if total > 0 else 0,
        "complexity_correct": complexity_correct,
        "model_usage": dict(model_usage),
        "reasoning_required": reasoning_required,
        "reasoning_percentage": reasoning_required / total if total > 0 else 0,
        "latency_stats": latency_stats,
        "o3_usage": o3_usage,
        "o3_percentage": o3_usage / total if total > 0 else 0,
    }


def print_accuracy_comparison(metrics: dict[str, dict[str, Any]]):
    """Print accuracy comparison table."""
    print("\n📊 ACCURACY COMPARISON")
    print("=" * 80)
    print(
        f"{'Approach':<25} {'Accuracy':<10} {'Correct/Total':<15} {'Latency (ms)':<15} {'O3 Usage':<10}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        accuracy = data["complexity_accuracy"]
        correct = data["complexity_correct"]
        total = data["total_tests"]
        latency = data["latency_stats"]["mean"]
        o3_usage = data["o3_usage"]

        print(
            f"{approach:<25} {accuracy:.1%} {correct}/{total:<15} {latency:.2f}ms {o3_usage}/{total}"
        )


def print_latency_analysis(metrics: dict[str, dict[str, Any]]):
    """Print detailed latency analysis."""
    print("\n⏱️  LATENCY ANALYSIS")
    print("=" * 80)
    print(
        f"{'Approach':<25} {'Mean':<10} {'Median':<10} {'Min':<10} {'Max':<10} {'Total':<10}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        stats = data["latency_stats"]
        print(
            f"{approach:<25} {stats['mean']:.2f}ms {stats['median']:.2f}ms {stats['min']:.2f}ms {stats['max']:.2f}ms {stats['total']:.2f}ms"
        )


def print_model_usage_analysis(metrics: dict[str, dict[str, Any]]):
    """Print model usage analysis."""
    print("\n🤖 MODEL USAGE ANALYSIS")
    print("=" * 80)

    for approach, data in metrics.items():
        print(f"\n{approach}:")
        model_usage = data["model_usage"]
        for model, count in sorted(
            model_usage.items(), key=lambda x: x[1], reverse=True
        ):
            percentage = count / data["total_tests"] * 100
            print(f"  {model}: {count} ({percentage:.1f}%)")


def print_cost_analysis(metrics: dict[str, dict[str, Any]]):
    """Print cost analysis."""
    print("\n💰 COST ANALYSIS")
    print("=" * 80)
    print(
        f"{'Approach':<25} {'O3 Usage':<10} {'O3 %':<10} {'Reasoning %':<12} {'Efficiency':<15}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        o3_usage = data["o3_usage"]
        o3_percentage = data["o3_percentage"]
        reasoning_percentage = data["reasoning_percentage"]

        # Efficiency: accuracy / (o3_percentage + 0.1) to avoid division by zero
        efficiency = data["complexity_accuracy"] / (o3_percentage + 0.1)

        print(
            f"{approach:<25} {o3_usage}/{data['total_tests']:<10} {o3_percentage:.1%} {reasoning_percentage:.1%} {efficiency:.3f}"
        )


def print_detailed_breakdown(
    results: list[dict[str, Any]], metrics: dict[str, dict[str, Any]]
):
    """Print detailed breakdown of results."""
    print("\n🔍 DETAILED BREAKDOWN")
    print("=" * 80)

    # Group by test case
    test_cases = defaultdict(list)
    for result in results:
        test_id = result["test_case"]["description"]
        test_cases[test_id].append(result)

    print(f"\nResults by test case ({len(test_cases)} cases):")
    for test_id, case_results in test_cases.items():
        print(f"\n{test_id}:")

        # Group by approach
        by_approach = defaultdict(list)
        for result in case_results:
            by_approach[result["approach"]].append(result)

        for approach, approach_results in by_approach.items():
            if approach_results:
                result = approach_results[0]  # Should be only one per approach
                complexity = result["complexity"]
                expected = result["expected_complexity"]
                latency = result["latency_ms"]
                correct = "✅" if result["complexity_correct"] else "❌"

                print(
                    f"  {approach:<20}: {complexity} (expected: {expected}) {correct} - {latency:.2f}ms"
                )


def print_recommendations(metrics: dict[str, dict[str, Any]]):
    """Print recommendations based on analysis."""
    print("\n💡 RECOMMENDATIONS")
    print("=" * 80)

    # Find best approaches
    approaches = list(metrics.keys())

    # Best accuracy
    best_accuracy = max(approaches, key=lambda a: metrics[a]["complexity_accuracy"])
    print(
        f"🎯 Best Accuracy: {best_accuracy} ({metrics[best_accuracy]['complexity_accuracy']:.1%})"
    )

    # Fastest
    fastest = min(approaches, key=lambda a: metrics[a]["latency_stats"]["mean"])
    print(
        f"⚡ Fastest: {fastest} ({metrics[fastest]['latency_stats']['mean']:.2f}ms avg)"
    )

    # Most cost-effective (lowest o3 usage with good accuracy)
    cost_effective = min(
        approaches,
        key=lambda a: metrics[a]["o3_percentage"]
        + (1 - metrics[a]["complexity_accuracy"]),
    )
    print(
        f"💰 Most Cost-Effective: {cost_effective} (O3: {metrics[cost_effective]['o3_percentage']:.1%}, Accuracy: {metrics[cost_effective]['complexity_accuracy']:.1%})"
    )

    # Best reasoning detection
    semantic_metrics = metrics.get("semantic")
    if semantic_metrics:
        reasoning_detected = semantic_metrics["reasoning_required"]
        print(
            f"🧠 Semantic approach detected {reasoning_detected} prompts requiring reasoning"
        )

    # Overall recommendation
    print("\n🏆 Overall Recommendation:")

    # If semantic is significantly better and not much slower
    semantic_metrics = metrics.get("semantic")
    algorithmic_metrics = metrics.get("algorithmic")

    if semantic_metrics and algorithmic_metrics:
        accuracy_diff = (
            semantic_metrics["complexity_accuracy"]
            - algorithmic_metrics["complexity_accuracy"]
        )
        latency_diff = (
            semantic_metrics["latency_stats"]["mean"]
            - algorithmic_metrics["latency_stats"]["mean"]
        )

        if (
            accuracy_diff > 0.1 and latency_diff < 100
        ):  # 10% better accuracy, <100ms slower
            print(
                "✅ Use Semantic approach - significantly better accuracy with acceptable latency"
            )
        elif accuracy_diff > 0.05:
            print("✅ Use Semantic approach - better accuracy")
        else:
            print("✅ Use Algorithmic approach - similar accuracy, faster")
    else:
        print("✅ Use the approach with best accuracy/cost trade-off")


def main():
    """Main analysis function."""
    parser = argparse.ArgumentParser(description="Analyze benchmark results")
    parser.add_argument("data_file", help="JSON file containing benchmark data")
    parser.add_argument(
        "--sections",
        nargs="+",
        choices=[
            "accuracy",
            "latency",
            "models",
            "cost",
            "breakdown",
            "recommendations",
        ],
        default=["accuracy", "latency", "models", "cost", "recommendations"],
        help="Analysis sections to include",
    )

    args = parser.parse_args()

    # Load data
    print(f"📂 Loading benchmark data from: {args.data_file}")
    data = load_benchmark_data(args.data_file)
    results = data["results"]

    print(f"📊 Loaded {len(results)} results from {data['metadata']['timestamp']}")
    print(f"🔧 Approaches: {', '.join(data['metadata']['approaches'])}")

    # Group by approach
    grouped_results = group_results_by_approach(results)

    # Calculate metrics for each approach
    metrics = {}
    for approach, approach_results in grouped_results.items():
        metrics[approach] = calculate_approach_metrics(approach_results, approach)

    # Print requested sections
    if "accuracy" in args.sections:
        print_accuracy_comparison(metrics)

    if "latency" in args.sections:
        print_latency_analysis(metrics)

    if "models" in args.sections:
        print_model_usage_analysis(metrics)

    if "cost" in args.sections:
        print_cost_analysis(metrics)

    if "breakdown" in args.sections:
        print_detailed_breakdown(results, metrics)

    if "recommendations" in args.sections:
        print_recommendations(metrics)

    print("\n" + "=" * 80)
    print("✅ Analysis completed!")


if __name__ == "__main__":
    main()
