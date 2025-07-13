import json
from collections import defaultdict
from pathlib import Path

RESULTS_FILE = Path(__file__).parent.parent / "benchmark_results.json"


def load_results():
    with open(RESULTS_FILE) as f:
        return json.load(f)


def analyze(results):
    by_model = defaultdict(list)
    for r in results:
        by_model[r["model"]].append(r)

    print("\n=== Per-Model Statistics ===")
    for model, entries in by_model.items():
        successes = [e for e in entries if e["success"]]
        failures = [e for e in entries if not e["success"]]
        n = len(entries)
        n_success = len(successes)
        n_fail = len(failures)
        avg_latency = (
            sum(e["latency_ms"] for e in successes) / n_success if n_success else 0
        )
        avg_tokens = (
            sum(e["total_tokens"] for e in successes) / n_success if n_success else 0
        )
        total_tokens = sum(e["total_tokens"] for e in successes)
        print(f"\nModel: {model}")
        print(f"  Success: {n_success}/{n} ({n_success / n * 100:.1f}%)")
        print(f"  Avg Latency: {avg_latency:.1f} ms")
        print(f"  Avg Tokens: {avg_tokens:.1f}")
        print(f"  Total Tokens: {total_tokens}")
        if n_fail:
            print(f"  Failures: {n_fail}")
            for fail in failures:
                print(f"    - Error: {fail.get('error')}")

    # Overall stats
    all_successes = [e for e in results if e["success"]]
    if all_successes:
        fastest = min(all_successes, key=lambda x: x["latency_ms"])
        slowest = max(all_successes, key=lambda x: x["latency_ms"])
        print("\n=== Overall Statistics ===")
        print(f"  Fastest: {fastest['model']} ({fastest['latency_ms']:.1f} ms)")
        print(f"  Slowest: {slowest['model']} ({slowest['latency_ms']:.1f} ms)")
        print(
            f"  Average Latency: {sum(e['latency_ms'] for e in all_successes) / len(all_successes):.1f} ms"
        )
        print(
            f"  Average Tokens: {sum(e['total_tokens'] for e in all_successes) / len(all_successes):.1f}"
        )
        print(f"  Total Tokens: {sum(e['total_tokens'] for e in all_successes)}")
    else:
        print("No successful results to analyze.")

    # Table
    print("\n=== Detailed Results Table ===")
    print(
        f"{'Model':<15} {'Prompt':<30} {'Latency(ms)':>12} {'Tokens':>8} {'Success':>8}"
    )
    for e in results:
        print(
            f"{e['model']:<15} {e['prompt'][:28]:<30} {e['latency_ms']:>12.1f} {e['total_tokens']:>8} {str(e['success']):>8}"
        )


def main():
    results = load_results()
    analyze(results)


if __name__ == "__main__":
    main()
