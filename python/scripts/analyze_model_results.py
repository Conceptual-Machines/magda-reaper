#!/usr/bin/env python3
"""
Script to analyze model performance test results.
"""

import json
import sys
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib.pyplot as plt

    # import pandas as pd  # Removed unused import
    PLOTTING_AVAILABLE = True
except ImportError:
    PLOTTING_AVAILABLE = False
    print(
        "⚠️  matplotlib/pandas not available. Install with: pip install matplotlib pandas"
    )


def load_results(filename: str) -> list[dict]:
    """Load test results from a JSON file."""
    with open(filename) as f:
        return json.load(f)


def analyze_results(results: list[dict]) -> dict:
    """Analyze the test results and return summary statistics."""
    analysis = {
        "total_tests": len(results),
        "models": defaultdict(list),
        "successful_tests": 0,
        "failed_tests": 0,
        "model_stats": {},
    }

    for result in results:
        model = result["model"]
        analysis["models"][model].append(result)

        if result["success"]:
            analysis["successful_tests"] += 1
        else:
            analysis["failed_tests"] += 1

    # Calculate statistics for each model
    for model, model_results in analysis["models"].items():
        successful = [r for r in model_results if r["success"]]
        failed = [r for r in model_results if not r["success"]]

        if successful:
            avg_latency = sum(r["latency_ms"] for r in successful) / len(successful)
            avg_tokens = sum(r["total_tokens"] for r in successful) / len(successful)
            min_latency = min(r["latency_ms"] for r in successful)
            max_latency = max(r["latency_ms"] for r in successful)
        else:
            avg_latency = avg_tokens = min_latency = max_latency = 0

        analysis["model_stats"][model] = {
            "total_tests": len(model_results),
            "successful": len(successful),
            "failed": len(failed),
            "success_rate": len(successful) / len(model_results)
            if model_results
            else 0,
            "avg_latency_ms": avg_latency,
            "avg_tokens": avg_tokens,
            "min_latency_ms": min_latency,
            "max_latency_ms": max_latency,
        }

    return analysis


def print_analysis(analysis: dict):
    """Print a formatted analysis of the results."""
    print("\n" + "=" * 80)
    print("MODEL PERFORMANCE ANALYSIS")
    print("=" * 80)

    print("\n📊 Overall Statistics:")
    print(f"   Total tests: {analysis['total_tests']}")
    print(f"   Successful: {analysis['successful_tests']}")
    print(f"   Failed: {analysis['failed_tests']}")
    print(
        f"   Success rate: {analysis['successful_tests'] / analysis['total_tests'] * 100:.1f}%"
    )

    print("\n🏆 Model Rankings:")

    # Sort models by average latency (successful tests only)
    successful_models = [
        (model, stats)
        for model, stats in analysis["model_stats"].items()
        if stats["successful"] > 0
    ]

    if successful_models:
        # Sort by average latency (fastest first)
        fastest_models = sorted(successful_models, key=lambda x: x[1]["avg_latency_ms"])

        print("\n⚡ Speed Ranking (Average Latency):")
        for i, (model, stats) in enumerate(fastest_models, 1):
            print(f"   {i}. {model}: {stats['avg_latency_ms']:.1f}ms")

        # Sort by success rate
        most_reliable = sorted(
            successful_models, key=lambda x: x[1]["success_rate"], reverse=True
        )

        print("\n✅ Reliability Ranking (Success Rate):")
        for i, (model, stats) in enumerate(most_reliable, 1):
            print(f"   {i}. {model}: {stats['success_rate'] * 100:.1f}%")

        # Sort by token efficiency
        most_efficient = sorted(successful_models, key=lambda x: x[1]["avg_tokens"])

        print("\n💰 Token Efficiency Ranking (Average Tokens):")
        for i, (model, stats) in enumerate(most_efficient, 1):
            print(f"   {i}. {model}: {stats['avg_tokens']:.1f} tokens")

    print("\n📋 Detailed Model Statistics:")
    for model, stats in analysis["model_stats"].items():
        print(f"\n   {model.upper()}:")
        print(
            f"     Tests: {stats['total_tests']} (✅ {stats['successful']}, ❌ {stats['failed']})"
        )
        print(f"     Success rate: {stats['success_rate'] * 100:.1f}%")
        if stats["successful"] > 0:
            print(
                f"     Latency: {stats['avg_latency_ms']:.1f}ms (min: {stats['min_latency_ms']:.1f}ms, max: {stats['max_latency_ms']:.1f}ms)"
            )
            print(f"     Tokens: {stats['avg_tokens']:.1f} average")


def create_visualizations(analysis: dict, output_dir: str = "results"):
    """Create visualizations of the results."""
    if not PLOTTING_AVAILABLE:
        print("⚠️  Skipping visualizations (matplotlib/pandas not available)")
        return

    # Create output directory
    Path(output_dir).mkdir(exist_ok=True)

    # Prepare data for plotting
    models = list(analysis["model_stats"].keys())
    latencies = [analysis["model_stats"][m]["avg_latency_ms"] for m in models]
    success_rates = [analysis["model_stats"][m]["success_rate"] * 100 for m in models]
    token_counts = [analysis["model_stats"][m]["avg_tokens"] for m in models]

    # Create subplots
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle("MAGDA Model Performance Analysis", fontsize=16)

    # 1. Latency comparison
    ax1.bar(models, latencies, color="skyblue")
    ax1.set_title("Average Latency by Model")
    ax1.set_ylabel("Latency (ms)")
    ax1.tick_params(axis="x", rotation=45)

    # 2. Success rate comparison
    ax2.bar(models, success_rates, color="lightgreen")
    ax2.set_title("Success Rate by Model")
    ax2.set_ylabel("Success Rate (%)")
    ax2.tick_params(axis="x", rotation=45)

    # 3. Token usage comparison
    ax3.bar(models, token_counts, color="lightcoral")
    ax3.set_title("Average Token Usage by Model")
    ax3.set_ylabel("Tokens")
    ax3.tick_params(axis="x", rotation=45)

    # 4. Latency vs Success Rate scatter plot
    ax4.scatter(latencies, success_rates, s=100, alpha=0.7)
    for i, model in enumerate(models):
        ax4.annotate(
            model,
            (latencies[i], success_rates[i]),
            xytext=(5, 5),
            textcoords="offset points",
            fontsize=8,
        )
    ax4.set_xlabel("Average Latency (ms)")
    ax4.set_ylabel("Success Rate (%)")
    ax4.set_title("Latency vs Success Rate")

    plt.tight_layout()
    plt.savefig(
        f"{output_dir}/model_performance_analysis.png", dpi=300, bbox_inches="tight"
    )
    print(f"📊 Visualizations saved to {output_dir}/model_performance_analysis.png")


def compare_results(file1: str, file2: str):
    """Compare results from two different test runs."""
    results1 = load_results(file1)
    results2 = load_results(file2)

    analysis1 = analyze_results(results1)
    analysis2 = analyze_results(results2)

    print(f"\n🔄 COMPARISON: {file1} vs {file2}")
    print("=" * 80)

    # Compare models that appear in both
    models1 = set(analysis1["model_stats"].keys())
    models2 = set(analysis2["model_stats"].keys())
    common_models = models1.intersection(models2)

    if common_models:
        print(f"\n📊 Models in both tests: {', '.join(common_models)}")

        for model in common_models:
            stats1 = analysis1["model_stats"][model]
            stats2 = analysis2["model_stats"][model]

            print(f"\n   {model.upper()}:")
            latency_diff = stats2["avg_latency_ms"] - stats1["avg_latency_ms"]
            success_diff = stats2["success_rate"] - stats1["success_rate"]

            print(
                f"     Latency: {stats1['avg_latency_ms']:.1f}ms → {stats2['avg_latency_ms']:.1f}ms ({latency_diff:+.1f}ms)"
            )
            print(
                f"     Success: {stats1['success_rate'] * 100:.1f}% → {stats2['success_rate'] * 100:.1f}% ({success_diff * 100:+.1f}%)"
            )
    else:
        print("❌ No common models found between the two test runs")


def main():
    """Main function to analyze results."""
    if len(sys.argv) < 2:
        print(
            "Usage: python scripts/analyze_model_results.py <results_file.json> [compare_file.json]"
        )
        print("\nExamples:")
        print(
            "  python scripts/analyze_model_results.py quick_test_results_20241201_143022.json"
        )
        print("  python scripts/analyze_model_results.py file1.json file2.json")
        sys.exit(1)

    filename = sys.argv[1]

    if not Path(filename).exists():
        print(f"❌ File not found: {filename}")
        sys.exit(1)

    print(f"📊 Analyzing results from: {filename}")
    results = load_results(filename)
    analysis = analyze_results(results)

    print_analysis(analysis)

    # Create visualizations if matplotlib is available
    create_visualizations(analysis)

    # Compare with another file if provided
    if len(sys.argv) > 2:
        compare_file = sys.argv[2]
        if Path(compare_file).exists():
            compare_results(filename, compare_file)
        else:
            print(f"⚠️  Comparison file not found: {compare_file}")
