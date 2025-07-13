#!/usr/bin/env python3
"""
Script to run model performance tests with different configurations.
"""

import os
import sys
import time
from datetime import datetime
from pathlib import Path

# Add the project root to the path
sys.path.insert(0, str(Path(__file__).parent.parent))

from tests.test_model_performance import (
    MODELS_TO_TEST,
    TEST_PROMPTS,
    ModelPerformanceTester,
)


def run_quick_test():
    """Run a quick test with limited models and prompts."""
    print("🚀 Running quick model performance test...")

    tester = ModelPerformanceTester(log_to_file=True)

    # Quick test: 2 models, 3 prompts
    quick_models = MODELS_TO_TEST[:2]
    quick_prompts = TEST_PROMPTS[:3]

    print(
        f"Testing {len(quick_models)} models with {len(quick_prompts)} prompts each..."
    )

    for model in quick_models:
        for prompt in quick_prompts:
            print(f"\nTesting {model} with: {prompt[:50]}...")
            result = tester.test_model_with_prompt(model, prompt)
            tester.results.append(result)

            # Small delay to avoid rate limits
            time.sleep(1)

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"quick_test_results_{timestamp}.json"
    tester.save_results(filename)
    tester.print_summary()


def run_full_test():
    """Run a full test with all models and prompts."""
    print("🧪 Running full model performance test...")

    tester = ModelPerformanceTester(log_to_file=True)

    print(
        f"Testing {len(MODELS_TO_TEST)} models with {len(TEST_PROMPTS)} prompts each..."
    )
    print("This will take some time and use API credits!")

    for i, model in enumerate(MODELS_TO_TEST):
        print(f"\n📊 Testing model {i + 1}/{len(MODELS_TO_TEST)}: {model}")

        for j, prompt in enumerate(TEST_PROMPTS):
            print(f"  Prompt {j + 1}/{len(TEST_PROMPTS)}: {prompt[:40]}...")
            result = tester.test_model_with_prompt(model, prompt)
            tester.results.append(result)

            # Delay to avoid rate limits
            time.sleep(2)

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"full_test_results_{timestamp}.json"
    tester.save_results(filename)
    tester.print_summary()


def run_latency_benchmark():
    """Run a focused latency benchmark."""
    print("⚡ Running latency benchmark...")

    tester = ModelPerformanceTester(log_to_file=True)
    simple_prompt = "Create a track called 'Test'"

    print(f"Testing all models with prompt: '{simple_prompt}'")

    for model in MODELS_TO_TEST:
        print(f"Testing {model}...")
        result = tester.test_model_with_prompt(model, simple_prompt)
        tester.results.append(result)

        print(f"  Latency: {result.latency_ms:.1f}ms")
        if not result.success:
            print(f"  Error: {result.error}")

        time.sleep(1)

    # Find fastest model
    successful_results = [r for r in tester.results if r.success]
    if successful_results:
        fastest = min(successful_results, key=lambda x: x.latency_ms)
        slowest = max(successful_results, key=lambda x: x.latency_ms)

        print("\n🏆 Results:")
        print(f"  Fastest: {fastest.model} ({fastest.latency_ms:.1f}ms)")
        print(f"  Slowest: {slowest.model} ({slowest.latency_ms:.1f}ms)")

        # Calculate average
        avg_latency = sum(r.latency_ms for r in successful_results) / len(
            successful_results
        )
        print(f"  Average: {avg_latency:.1f}ms")

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"latency_benchmark_{timestamp}.json"
    tester.save_results(filename)


def run_custom_test(models=None, prompts=None):
    """Run a custom test with specified models and prompts."""
    if models is None:
        models = MODELS_TO_TEST[:3]
    if prompts is None:
        prompts = TEST_PROMPTS[:5]

    print(
        f"🔧 Running custom test with {len(models)} models and {len(prompts)} prompts..."
    )

    tester = ModelPerformanceTester(log_to_file=True)

    for model in models:
        for prompt in prompts:
            print(f"Testing {model} with: {prompt[:50]}...")
            result = tester.test_model_with_prompt(model, prompt)
            tester.results.append(result)
            time.sleep(1)

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"custom_test_results_{timestamp}.json"
    tester.save_results(filename)
    tester.print_summary()


def main():
    """Main function to run different types of tests."""
    if not os.getenv("OPENAI_API_KEY"):
        print("❌ Error: OPENAI_API_KEY environment variable not set")
        print("Please set your OpenAI API key before running tests")
        sys.exit(1)

    print("🎵 MAGDA Model Performance Testing")
    print("=" * 50)

    if len(sys.argv) > 1:
        test_type = sys.argv[1].lower()

        if test_type == "quick":
            run_quick_test()
        elif test_type == "full":
            run_full_test()
        elif test_type == "latency":
            run_latency_benchmark()
        elif test_type == "custom":
            # You can customize models and prompts here
            custom_models = ["gpt-4o-mini", "o3-mini"]
            custom_prompts = ["Create a track called 'Bass'", "Add reverb to the track"]
            run_custom_test(custom_models, custom_prompts)
        else:
            print(f"❌ Unknown test type: {test_type}")
            print_usage()
    else:
        print_usage()


def print_usage():
    """Print usage information."""
    print("\nUsage:")
    print("  python scripts/run_model_performance_tests.py [test_type]")
    print("\nTest types:")
    print("  quick    - Quick test (2 models, 3 prompts)")
    print("  full     - Full test (all models, all prompts)")
    print("  latency  - Latency benchmark only")
    print("  custom   - Custom test with specific models/prompts")
    print("\nExample:")
    print("  python scripts/run_model_performance_tests.py quick")


if __name__ == "__main__":
    main()
