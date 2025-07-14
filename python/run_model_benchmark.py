#!/usr/bin/env python3
"""
Standalone script to run model performance benchmark without pytest.
"""

import json
import os
import sys
import time
from pathlib import Path

from dotenv import load_dotenv

from magda.prompt_loader import get_prompt

# Add the project root to the path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

# Load environment variables from .env file
load_dotenv("../.env")

# Load operation identifier prompt
OPERATION_IDENTIFIER_PROMPT = get_prompt("operation_identifier")


def test_model_performance(model_name, prompt, client):
    """Test a single model and collect performance metrics."""
    print(f"DEBUG: Starting test for {model_name} with prompt: {prompt[:50]}...")

    start_time = time.time()

    try:
        print(f"DEBUG: Making direct API call to {model_name}")

        # Make direct API call to the operation identifier stage
        response = client.responses.create(
            input=prompt, instructions=OPERATION_IDENTIFIER_PROMPT, model=model_name
        )

        end_time = time.time()
        latency = (end_time - start_time) * 1000  # Convert to milliseconds

        print(f"DEBUG: Response type: {type(response)}")
        print(f"DEBUG: Response attributes: {dir(response)}")

        # Extract token usage - check the actual structure
        prompt_tokens = 0
        completion_tokens = 0
        total_tokens = 0

        if hasattr(response, "usage") and response.usage:
            usage = response.usage
            print(f"DEBUG: Usage object type: {type(usage)}")
            print(f"DEBUG: Usage object attributes: {dir(usage)}")

            # Try different possible attribute names
            if hasattr(usage, "prompt_tokens"):
                prompt_tokens = usage.prompt_tokens
            elif hasattr(usage, "input_tokens"):
                prompt_tokens = usage.input_tokens
            elif hasattr(usage, "input"):
                prompt_tokens = usage.input

            if hasattr(usage, "completion_tokens"):
                completion_tokens = usage.completion_tokens
            elif hasattr(usage, "output_tokens"):
                completion_tokens = usage.output_tokens
            elif hasattr(usage, "output"):
                completion_tokens = usage.output

            if hasattr(usage, "total_tokens"):
                total_tokens = usage.total_tokens
            elif hasattr(usage, "total"):
                total_tokens = usage.total
            else:
                total_tokens = prompt_tokens + completion_tokens

        # Parse the response
        try:
            if hasattr(response, "output") and response.output:
                # For models with structured output
                result = response.output
            elif hasattr(response, "text") and response.text:
                # For models with free-form text output
                result = response.text
            else:
                result = str(response)

            # Try to parse as JSON if it's a string
            if isinstance(result, str):
                try:
                    result = json.loads(result)
                except json.JSONDecodeError:
                    pass  # Keep as string if not valid JSON

            # Convert result to JSON-serializable format
            if hasattr(result, "to_dict"):
                result = result.to_dict()
            elif hasattr(result, "dict"):
                result = result.dict()
            elif hasattr(result, "__dict__"):
                result = result.__dict__
            else:
                result = str(result)

            return {
                "model": model_name,
                "prompt": prompt,
                "latency_ms": latency,
                "prompt_tokens": prompt_tokens,
                "completion_tokens": completion_tokens,
                "total_tokens": total_tokens,
                "success": True,
                "result": result,
            }

        except Exception as e:
            print(f"DEBUG: Exception occurred: {e}")
            return {
                "model": model_name,
                "prompt": prompt,
                "latency_ms": latency,
                "prompt_tokens": prompt_tokens,
                "completion_tokens": completion_tokens,
                "total_tokens": total_tokens,
                "success": False,
                "error": str(e),
            }

    except Exception as e:
        end_time = time.time()
        latency = (end_time - start_time) * 1000
        print(f"DEBUG: Exception occurred: {e}")
        return {
            "model": model_name,
            "prompt": prompt,
            "latency_ms": latency,
            "prompt_tokens": 0,
            "completion_tokens": 0,
            "total_tokens": 0,
            "success": False,
            "error": str(e),
        }


def get_supported_models(client):
    """Get all models that support the Responses API."""
    # These are the models we've confirmed support the Responses API
    supported_models = [
        "gpt-4.1",
        "gpt-4.1-mini",
        "gpt-4.1-nano",
        "gpt-4o",
        "gpt-4o-mini",
        "o3-mini",
        "o1",
        "o1-pro",
        "o4-mini",
    ]

    # Filter to only include models that are actually available
    available_models = []
    try:
        all_models = client.models.list()
        available_model_ids = [model.id for model in all_models.data]

        for model in supported_models:
            if model in available_model_ids:
                available_models.append(model)
            else:
                print(f"⚠️  Model {model} not available in your account")

    except Exception as e:
        print(f"⚠️  Could not fetch available models: {e}")
        # Fall back to the known supported models
        available_models = supported_models

    return available_models


def main():
    """Run the model performance benchmark."""
    import argparse

    from dotenv import load_dotenv
    from openai import OpenAI

    # Load environment variables
    load_dotenv()

    parser = argparse.ArgumentParser(description="Run model performance benchmark")
    parser.add_argument(
        "--config",
        choices=["quick", "full", "latency", "multistep", "custom"],
        default="quick",
        help="Test configuration",
    )
    parser.add_argument("--models", nargs="+", help="Specific models to test")
    parser.add_argument("--prompts", nargs="+", help="Specific prompts to test")
    parser.add_argument(
        "--output", default="benchmark_results.json", help="Output file for results"
    )

    args = parser.parse_args()

    # Initialize OpenAI client
    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        print("❌ OPENAI_API_KEY not found in environment variables")
        return 1

    client = OpenAI(api_key=api_key)

    # Get available models
    all_available_models = get_supported_models(client)

    # Define test configurations
    configs = {
        "quick": {
            "models": ["gpt-4o-mini", "o3-mini"],
            "prompts": ["Create a track called 'Test'"],
        },
        "full": {
            "models": all_available_models,  # Use all available models
            "prompts": [
                "Create a track called 'Test'",
                "Add a compressor to the bass track with 4:1 ratio",
                "Set the volume of the drums to -6dB",
            ],
        },
        "latency": {
            "models": ["gpt-4o-mini", "o3-mini", "o4-mini"],
            "prompts": ["Create a track called 'Test'"],
        },
        "multistep": {
            "models": all_available_models,
            "prompts": [
                "Create a track called 'Drums', add a 4-bar drum loop clip, and set the volume to -3dB",
                "Create a bass track, record a 2-bar bass line, add compression with 3:1 ratio, and automate the volume to fade in over 8 bars",
                "Set up a vocal track with reverb, record a verse, then adjust the mix so the vocals sit well with the drums and bass",
            ],
        },
        "custom": {
            "models": args.models or ["gpt-4o-mini"],
            "prompts": args.prompts or ["Create a track called 'Test'"],
        },
    }

    config = configs[args.config]
    models = config["models"]
    prompts = config["prompts"]

    # Filter models to only include available ones
    models = [m for m in models if m in all_available_models]

    if not models:
        print("❌ No supported models available for testing")
        return 1

    print(
        f"🚀 Running {args.config} benchmark with {len(models)} models and {len(prompts)} prompts"
    )
    print(f"📊 Models: {', '.join(models)}")
    print(f"💬 Prompts: {', '.join(prompts)}")
    print()

    results = []
    total_tests = len(models) * len(prompts)
    current_test = 0

    for model in models:
        for prompt in prompts:
            current_test += 1
            print(f"[{current_test}/{total_tests}] Testing {model}...")

            result = test_model_performance(model, prompt, client)
            results.append(result)

            if result["success"]:
                print(
                    f"   ✅ {model}: {result['latency_ms']:.1f}ms, {result['total_tokens']} tokens"
                )
            else:
                print(f"   ❌ {model}: Failed - {result['error']}")
            print()

    # Save results
    output_file = Path(args.output)
    with open(output_file, "w") as f:
        json.dump(results, f, indent=2)

    print("=" * 50)
    print("BENCHMARK RESULTS")
    print("=" * 50)

    successful_results = [r for r in results if r["success"]]
    if successful_results:
        # Calculate statistics
        latencies = [r["latency_ms"] for r in successful_results]
        total_tokens = [r["total_tokens"] for r in successful_results]

        print(f"✅ Successful tests: {len(successful_results)}/{len(results)}")
        print(f"📊 Average latency: {sum(latencies) / len(latencies):.1f}ms")
        print(f"📊 Min latency: {min(latencies):.1f}ms")
        print(f"📊 Max latency: {max(latencies):.1f}ms")
        print(f"🔢 Average tokens: {sum(total_tokens) / len(total_tokens):.1f}")
        print(f"🔢 Total tokens: {sum(total_tokens)}")
        print()

        # Group by model
        by_model = {}
        for result in successful_results:
            model = result["model"]
            if model not in by_model:
                by_model[model] = []
            by_model[model].append(result)

        print("📈 Results by model:")
        for model, model_results in by_model.items():
            model_latencies = [r["latency_ms"] for r in model_results]
            model_tokens = [r["total_tokens"] for r in model_results]
            print(
                f"  {model}: {sum(model_latencies) / len(model_latencies):.1f}ms avg, {sum(model_tokens)} total tokens"
            )
    else:
        print("❌ No successful results to report")

    print(f"\n💾 Results saved to: {output_file}")
    return 0


if __name__ == "__main__":
    exit(main())
