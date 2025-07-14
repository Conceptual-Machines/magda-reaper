#!/usr/bin/env python3
"""
Operations Benchmark Analysis Script

Analyzes operations benchmark results focusing on:
- Success rates and command generation
- Real-world performance metrics
- Pipeline effectiveness
- Error analysis
"""

import argparse
import json
from collections import defaultdict
from statistics import mean, median
from typing import Any


def load_operations_data(filename: str) -> dict[str, Any]:
    """Load operations benchmark data from JSON file."""
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


def calculate_operations_metrics(
    results: list[dict[str, Any]], approach_name: str
) -> dict[str, Any]:
    """Calculate comprehensive metrics for an operations approach."""
    total = len(results)

    # Success analysis (for pipeline approach)
    if approach_name == "pipeline":
        successful = sum(1 for r in results if r.get("success", False))
        success_rate = successful / total if total > 0 else 0

        # Command generation analysis
        command_counts = [r.get("command_count", 0) for r in results]
        avg_commands = mean(command_counts) if command_counts else 0
        total_commands = sum(command_counts)

        # Error analysis
        errors = [
            r.get("error", "")
            for r in results
            if not r.get("success", False) and r.get("error")
        ]
        error_types = defaultdict(int)
        for error in errors:
            error_types[error] += 1
    else:
        success_rate = None
        avg_commands = None
        total_commands = None
        error_types = {}

    # Complexity accuracy
    complexity_correct = sum(1 for r in results if r.get("complexity_correct", False))
    complexity_accuracy = complexity_correct / total if total > 0 else 0

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
        "success_rate": success_rate,
        "avg_commands": avg_commands,
        "total_commands": total_commands,
        "error_types": dict(error_types),
        "complexity_accuracy": complexity_accuracy,
        "complexity_correct": complexity_correct,
        "model_usage": dict(model_usage),
        "reasoning_required": reasoning_required,
        "reasoning_percentage": reasoning_required / total if total > 0 else 0,
        "latency_stats": latency_stats,
        "o3_usage": o3_usage,
        "o3_percentage": o3_usage / total if total > 0 else 0,
    }


def print_success_analysis(metrics: dict[str, dict[str, Any]]):
    """Print success rate analysis."""
    print("\n🎯 SUCCESS ANALYSIS")
    print("=" * 80)
    print(
        f"{'Approach':<20} {'Success Rate':<15} {'Avg Commands':<15} {'Total Commands':<15}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        if data["success_rate"] is not None:
            success_rate = data["success_rate"]
            avg_commands = data["avg_commands"]
            total_commands = data["total_commands"]

            print(
                f"{approach:<20} {success_rate:.1%} {avg_commands:.1f} {total_commands}"
            )


def print_complexity_accuracy(metrics: dict[str, dict[str, Any]]):
    """Print complexity accuracy comparison."""
    print("\n📊 COMPLEXITY ACCURACY")
    print("=" * 80)
    print(
        f"{'Approach':<20} {'Accuracy':<10} {'Correct/Total':<15} {'Latency (ms)':<15}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        if data["complexity_accuracy"] is not None:
            accuracy = data["complexity_accuracy"]
            correct = data["complexity_correct"]
            total = data["total_tests"]
            latency = data["latency_stats"]["mean"]

            print(
                f"{approach:<20} {accuracy:.1%} {correct}/{total:<15} {latency:.2f}ms"
            )


def print_latency_analysis(metrics: dict[str, dict[str, Any]]):
    """Print detailed latency analysis."""
    print("\n⏱️  LATENCY ANALYSIS")
    print("=" * 80)
    print(
        f"{'Approach':<20} {'Mean':<10} {'Median':<10} {'Min':<10} {'Max':<10} {'Total':<10}"
    )
    print("-" * 80)

    for approach, data in metrics.items():
        stats = data["latency_stats"]
        print(
            f"{approach:<20} {stats['mean']:.2f}ms {stats['median']:.2f}ms {stats['min']:.2f}ms {stats['max']:.2f}ms {stats['total']:.2f}ms"
        )


def print_error_analysis(metrics: dict[str, dict[str, Any]]):
    """Print error analysis for pipeline approach."""
    print("\n❌ ERROR ANALYSIS")
    print("=" * 80)

    pipeline_metrics = metrics.get("pipeline")
    if pipeline_metrics and pipeline_metrics["error_types"]:
        print("Pipeline errors by type:")
        for error_type, count in pipeline_metrics["error_types"].items():
            print(f"  {error_type}: {count} occurrences")
    else:
        print("No errors recorded or no pipeline results available")


def print_command_analysis(results: list[dict[str, Any]]):
    """Print detailed command generation analysis."""
    print("\n🔧 COMMAND GENERATION ANALYSIS")
    print("=" * 80)

    # Group by test case
    test_cases = defaultdict(list)
    for result in results:
        if result["approach"] == "pipeline":
            test_id = result["test_case"]["description"]
            test_cases[test_id].append(result)

    print(f"\nPipeline results by test case ({len(test_cases)} cases):")
    for test_id, case_results in test_cases.items():
        if case_results:
            result = case_results[0]  # Should be only one per test case
            success = result["success"]
            command_count = result["command_count"]
            latency = result["latency_ms"]

            status = "✅" if success else "❌"
            print(f"\n{test_id}:")
            print(f"  Success: {status}")
            print(f"  Commands: {command_count}")
            print(f"  Latency: {latency:.2f}ms")

            if result["daw_commands"]:
                print("  Generated commands:")
                for i, cmd in enumerate(result["daw_commands"][:3], 1):
                    print(f"    {i}. {cmd}")
                if len(result["daw_commands"]) > 3:
                    print(f"    ... and {len(result['daw_commands']) - 3} more")


def print_recommendations(metrics: dict[str, dict[str, Any]]):
    """Print recommendations based on operations analysis."""
    print("\n💡 RECOMMENDATIONS")
    print("=" * 80)

    # Find best approaches
    approaches = list(metrics.keys())

    # Best complexity accuracy
    complexity_approaches = [
        a for a in approaches if metrics[a]["complexity_accuracy"] is not None
    ]
    if complexity_approaches:
        best_accuracy = max(
            complexity_approaches, key=lambda a: metrics[a]["complexity_accuracy"]
        )
        print(
            f"🎯 Best Complexity Accuracy: {best_accuracy} ({metrics[best_accuracy]['complexity_accuracy']:.1%})"
        )

    # Fastest
    fastest = min(approaches, key=lambda a: metrics[a]["latency_stats"]["mean"])
    print(
        f"⚡ Fastest: {fastest} ({metrics[fastest]['latency_stats']['mean']:.2f}ms avg)"
    )

    # Pipeline success
    pipeline_metrics = metrics.get("pipeline")
    if pipeline_metrics and pipeline_metrics["success_rate"] is not None:
        success_rate = pipeline_metrics["success_rate"]
        avg_commands = pipeline_metrics["avg_commands"]
        print(f"🚀 Pipeline Success Rate: {success_rate:.1%}")
        print(f"📝 Average Commands Generated: {avg_commands:.1f}")

        if success_rate < 0.8:
            print(
                "⚠️  Pipeline success rate is below 80% - consider improving error handling"
            )
        elif success_rate > 0.9:
            print("✅ Pipeline success rate is excellent!")

    # Cost effectiveness
    semantic_metrics = metrics.get("semantic")
    if semantic_metrics:
        o3_percentage = semantic_metrics["o3_percentage"]
        print(f"💰 Semantic approach uses o3-mini {o3_percentage:.1%} of the time")

    # Overall recommendation
    print("\n🏆 Overall Recommendation:")

    if pipeline_metrics and pipeline_metrics["success_rate"] is not None:
        if pipeline_metrics["success_rate"] > 0.8:
            print("✅ Pipeline is working well - ready for production use")
        else:
            print("⚠️  Pipeline needs improvement - investigate error patterns")

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

        if accuracy_diff > 0.2:  # 20% better accuracy
            print("✅ Use Semantic approach - significantly better accuracy")
        elif accuracy_diff > 0.1:  # 10% better accuracy
            print("✅ Use Semantic approach - better accuracy with acceptable latency")
        else:
            print("✅ Use Algorithmic approach - similar accuracy, faster")


def main():
    """Main analysis function."""
    parser = argparse.ArgumentParser(description="Analyze operations benchmark results")
    parser.add_argument(
        "data_file", help="JSON file containing operations benchmark data"
    )
    parser.add_argument(
        "--sections",
        nargs="+",
        choices=[
            "success",
            "accuracy",
            "latency",
            "errors",
            "commands",
            "recommendations",
        ],
        default=[
            "success",
            "accuracy",
            "latency",
            "errors",
            "commands",
            "recommendations",
        ],
        help="Analysis sections to include",
    )

    args = parser.parse_args()

    # Load data
    print(f"📂 Loading operations benchmark data from: {args.data_file}")
    data = load_operations_data(args.data_file)
    results = data["results"]

    print(f"📊 Loaded {len(results)} results from {data['metadata']['timestamp']}")
    print(f"🔧 Approaches: {', '.join(data['metadata']['approaches'])}")

    # Group by approach
    grouped_results = group_results_by_approach(results)

    # Calculate metrics for each approach
    metrics = {}
    for approach, approach_results in grouped_results.items():
        metrics[approach] = calculate_operations_metrics(approach_results, approach)

    # Print requested sections
    if "success" in args.sections:
        print_success_analysis(metrics)

    if "accuracy" in args.sections:
        print_complexity_accuracy(metrics)

    if "latency" in args.sections:
        print_latency_analysis(metrics)

    if "errors" in args.sections:
        print_error_analysis(metrics)

    if "commands" in args.sections:
        print_command_analysis(results)

    if "recommendations" in args.sections:
        print_recommendations(metrics)

    print("\n" + "=" * 80)
    print("✅ Operations analysis completed!")


if __name__ == "__main__":
    main()
