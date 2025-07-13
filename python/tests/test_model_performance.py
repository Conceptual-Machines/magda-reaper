"""
Model Performance Testing Framework

Tests multiple models in a matrix to collect latency and token usage data.
"""

import json
import logging
import os
import time
from dataclasses import asdict, dataclass
from datetime import datetime

import pytest
from dotenv import load_dotenv
from openai import OpenAI

# Load environment variables from .env file
load_dotenv("../.env")


@dataclass
class ModelTestResult:
    """Results from a single model test."""

    model: str
    prompt: str
    latency_ms: float
    input_tokens: int
    output_tokens: int
    total_tokens: int
    success: bool
    error: str = ""
    daw_commands: list[str] | None = None
    operations_count: int = 0


class ModelPerformanceTester:
    """Framework for testing model performance across different models."""

    def __init__(self, log_to_file: bool = True):
        # Use the same approach as working integration tests
        api_key = os.getenv("OPENAI_API_KEY")
        print(
            f"DEBUG: Loaded API key: {api_key[:20]}..."
            if api_key
            else "DEBUG: No API key found"
        )

        self.client = OpenAI(api_key=api_key)
        self.results: list[ModelTestResult] = []

        # Setup logging
        if log_to_file:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            log_filename = f"model_performance_test_{timestamp}.log"

            # Configure logging
            logging.basicConfig(
                level=logging.DEBUG,
                format="%(asctime)s - %(levelname)s - %(message)s",
                handlers=[
                    logging.FileHandler(log_filename),
                    logging.StreamHandler(),  # Also log to console
                ],
            )
            self.logger = logging.getLogger(__name__)
            print(f"Model Performance Test started - Logging to {log_filename}")
        else:
            self.logger = logging.getLogger(__name__)

    def test_model_with_prompt(self, model: str, prompt: str) -> ModelTestResult:
        """Test a single model with a prompt and collect metrics."""
        print(f"DEBUG: Starting test for {model} with prompt: {prompt[:50]}...")
        start_time = time.time()

        try:
            # Make a direct API call to test the model
            from magda.prompt_loader import get_prompt

            instructions = get_prompt("operation_identifier")

            print(f"DEBUG: Making direct API call to {model}")
            response = self.client.responses.create(
                model=model, instructions=instructions, input=prompt
            )

            print(f"DEBUG: Response type: {type(response)}")
            print(f"DEBUG: Response attributes: {dir(response)}")

            # Capture usage from response
            input_tokens = 0
            output_tokens = 0
            if hasattr(response, "usage") and response.usage:
                input_tokens = response.usage.prompt_tokens or 0
                output_tokens = response.usage.completion_tokens or 0
                print(
                    f"DEBUG: Usage found - input: {input_tokens}, output: {output_tokens}"
                )
            else:
                print("DEBUG: No usage info found in response")
                if hasattr(response, "usage"):
                    print(f"DEBUG: Response.usage exists: {response.usage}")
                else:
                    print("DEBUG: Response has no usage attribute")

            # Parse the response
            operations = []
            if hasattr(response, "output_text") and response.output_text:
                try:
                    result = json.loads(response.output_text)
                    ops = result.get("operations", [])
                    if isinstance(ops, list):
                        operations = ops
                except Exception as e:
                    print(f"Error parsing response: {e}")

            end_time = time.time()
            latency_ms = (end_time - start_time) * 1000

            print(
                f"DEBUG: Final token counts - input: {input_tokens}, output: {output_tokens}"
            )

            return ModelTestResult(
                model=model,
                prompt=prompt,
                latency_ms=latency_ms,
                input_tokens=input_tokens,
                output_tokens=output_tokens,
                total_tokens=input_tokens + output_tokens,
                success=True,
                daw_commands=[],
                operations_count=len(operations),
            )

        except Exception as e:
            end_time = time.time()
            latency_ms = (end_time - start_time) * 1000

            print(f"DEBUG: Exception occurred: {e}")

            return ModelTestResult(
                model=model,
                prompt=prompt,
                latency_ms=latency_ms,
                input_tokens=0,
                output_tokens=0,
                total_tokens=0,
                success=False,
                error=str(e),
            )

    def save_results(self, filename: str = "model_performance_results.json"):
        """Save test results to a JSON file."""
        results_dict = [asdict(result) for result in self.results]

        with open(filename, "w") as f:
            json.dump(results_dict, f, indent=2)

        self.logger.info(f"Results saved to {filename}")
        print(f"Results saved to {filename}")

    def print_summary(self):
        """Print a summary of the test results."""
        if not self.results:
            self.logger.warning("No results to summarize")
            print("No results to summarize")
            return

        summary = "\n" + "=" * 80 + "\n"
        summary += "MODEL PERFORMANCE TEST SUMMARY\n"
        summary += "=" * 80 + "\n"

        # Group by model
        model_results = {}
        for result in self.results:
            if result.model not in model_results:
                model_results[result.model] = []
            model_results[result.model].append(result)

        for model, results in model_results.items():
            successful = [r for r in results if r.success]
            failed = [r for r in results if not r.success]

            summary += f"\n📊 {model.upper()}\n"
            summary += f"   Total tests: {len(results)}\n"
            summary += f"   Successful: {len(successful)}\n"
            summary += f"   Failed: {len(failed)}\n"

            if successful:
                avg_latency = sum(r.latency_ms for r in successful) / len(successful)
                avg_input_tokens = sum(r.input_tokens for r in successful) / len(
                    successful
                )
                avg_output_tokens = sum(r.output_tokens for r in successful) / len(
                    successful
                )
                avg_total_tokens = sum(r.total_tokens for r in successful) / len(
                    successful
                )
                summary += f"   Avg latency: {avg_latency:.1f}ms\n"
                summary += f"   Avg input tokens: {avg_input_tokens:.1f}\n"
                summary += f"   Avg output tokens: {avg_output_tokens:.1f}\n"
                summary += f"   Avg total tokens: {avg_total_tokens:.1f}\n"

        summary += "\n" + "=" * 80 + "\n"

        self.logger.info(summary)
        print(summary)


# Test prompts covering different scenarios
TEST_PROMPTS = [
    "Create a new track called 'Bass'",
    "Set the volume of track 'Bass' to -3dB",
    "Add reverb to the 'Bass' track",
    "Create a track for drums and one for guitar",
    "Crea una nueva pista llamada 'Piano Español'",
    "Ajoute une piste audio nommée 'Basse Française'",
    "Create a MIDI track with Serum VST",
    "Add a compressor with 4:1 ratio and -20dB threshold",
    "Fade out the guitar track over 4 bars",
    "Create a bus track for all guitars and add delay",
]

# Models to test (you can adjust this list)
MODELS_TO_TEST = [
    # GPT-4.1 family
    "gpt-4.1",
    "gpt-4.1-mini",
    "gpt-4.1-nano",
    # GPT-4o family
    "gpt-4o",
    "gpt-4o-mini",
    # O3 family
    "o3-mini",
    "o1-mini",
    "o1",
    "o1-pro",
    # O4 family
    "o4-mini",
    # Legacy GPT models
    "gpt-4",
    "gpt-4-turbo",
    "gpt-3.5-turbo",
    # Claude models (if available)
    "claude-3-5-sonnet-20241022",
    "claude-3-5-haiku-20241022",
]


# For faster testing, you can reduce these lists
@pytest.mark.parametrize(
    "model", MODELS_TO_TEST[:3]
)  # Limit to 3 models for faster testing
@pytest.mark.parametrize(
    "prompt", TEST_PROMPTS[:5]
)  # Limit to 5 prompts for faster testing
@pytest.mark.integration
def test_model_performance(model: str, prompt: str):
    """Test model performance with different prompts."""
    tester = ModelPerformanceTester()
    result = tester.test_model_with_prompt(model, prompt)
    tester.results.append(result)

    # Assert basic success (adjust as needed)
    if not result.success:
        pytest.skip(f"Model {model} failed: {result.error}")

    # Basic assertions
    assert result.latency_ms > 0
    assert result.total_tokens > 0
    assert result.operations_count >= 0

    # Print result for this test
    print(f"\n✅ {model} | {prompt[:50]}...")
    print(f"   Latency: {result.latency_ms:.1f}ms")
    print(f"   Input tokens: {result.input_tokens}")
    print(f"   Output tokens: {result.output_tokens}")
    print(f"   Total tokens: {result.total_tokens}")
    print(f"   Operations: {result.operations_count}")


@pytest.mark.integration
def test_model_comparison():
    """Run a comprehensive comparison of all models."""
    tester = ModelPerformanceTester()

    # Test each model with a subset of prompts for efficiency
    test_prompts = TEST_PROMPTS[:3]  # Use first 3 prompts
    test_models = MODELS_TO_TEST[:2]  # Use first 2 models

    print(
        f"\n🧪 Testing {len(test_models)} models with {len(test_prompts)} prompts each..."
    )

    for model in test_models:
        for prompt in test_prompts:
            result = tester.test_model_with_prompt(model, prompt)
            tester.results.append(result)

            # Small delay to avoid rate limits
            time.sleep(1)

    # Save and display results
    tester.save_results()
    tester.print_summary()


@pytest.mark.integration
def test_latency_benchmark():
    """Benchmark latency across models with a simple prompt."""
    tester = ModelPerformanceTester()

    simple_prompt = "Create a track called 'Test'"

    print(f"\n⚡ Latency benchmark with prompt: '{simple_prompt}'")

    # Test a good selection of models for benchmarking
    benchmark_models = [
        "gpt-4.1",
        "gpt-4.1-mini",
        "gpt-4.1-nano",
        "gpt-4o",
        "gpt-4o-mini",
        "o3-mini",
        "o1-mini",
        "o1",
        "o1-pro",
        "o4-mini",
    ]

    for model in benchmark_models:
        result = tester.test_model_with_prompt(model, simple_prompt)
        tester.results.append(result)

        print(f"   {model}: {result.latency_ms:.1f}ms ({result.total_tokens} tokens)")

        # Small delay to avoid rate limits
        time.sleep(1)

    # Find fastest model
    successful_results = [r for r in tester.results if r.success]
    if successful_results:
        fastest = min(successful_results, key=lambda x: x.latency_ms)
        print(f"\n🏆 Fastest model: {fastest.model} ({fastest.latency_ms:.1f}ms)")


if __name__ == "__main__":
    # Run the comparison test
    test_model_comparison()
