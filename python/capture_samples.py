#!/usr/bin/env python3
"""
Script to capture sample request/response data from integration tests.
Useful for debugging and comparing with C++ implementation.
"""

import json
from datetime import datetime

from dotenv import load_dotenv

from magda.pipeline import MAGDAPipeline

load_dotenv()


def capture_sample_data():
    """Capture sample request/response data from various test scenarios."""

    # Initialize pipeline
    pipeline = MAGDAPipeline()

    # Test scenarios
    test_scenarios = [
        {
            "name": "basic_track_creation",
            "prompt": "Create a new track called 'Electric Guitar'",
            "expected_type": "track",
        },
        {
            "name": "volume_adjustment",
            "prompt": "Set the volume of track 'Guitar' to -6dB",
            "expected_type": "volume",
        },
        {
            "name": "effect_addition",
            "prompt": "Add reverb to the 'Guitar' track",
            "expected_type": "effect",
        },
        {
            "name": "clip_creation",
            "prompt": "Create a 4-bar clip on track 'Guitar'",
            "expected_type": "clip",
        },
        {
            "name": "midi_operation",
            "prompt": "Add a C major chord at bar 1 on 'Piano'",
            "expected_type": "midi",
        },
        {
            "name": "multilingual_spanish",
            "prompt": "Crea una nueva pista llamada 'Guitarra Española'",
            "expected_type": "track",
        },
        {
            "name": "complex_multi_operation",
            "prompt": "Create a new track called 'Lead Guitar', set its volume to -3dB, and add reverb with 40% wet level",
            "expected_type": ["track", "volume", "effect"],
        },
    ]

    samples = []

    for scenario in test_scenarios:
        print(f"\n{'=' * 60}")
        print(f"Testing: {scenario['name']}")
        print(f"Prompt: {scenario['prompt']}")
        print(f"{'=' * 60}")

        try:
            # Process the prompt
            result = pipeline.process_prompt(scenario["prompt"])

            # Capture the sample data
            sample = {
                "timestamp": datetime.now().isoformat(),
                "scenario": scenario["name"],
                "prompt": scenario["prompt"],
                "expected_type": scenario["expected_type"],
                "result": {
                    "success": len(result.get("operations", [])) > 0,
                    "operations_count": len(result.get("operations", [])),
                    "operations": result.get("operations", []),
                    "daw_commands": result.get("daw_commands", []),
                    "results": [
                        {
                            "daw_command": r.get("daw_command"),
                            "result": {
                                k: v
                                for k, v in r.get("result", {}).items()
                                if not k.startswith("_")
                            },
                            "context": {
                                k: v
                                for k, v in r.get("context", {}).items()
                                if not isinstance(v, object)
                            },
                        }
                        for r in result.get("results", [])
                    ],
                    "context_summary": result.get("context_summary", {}),
                },
            }

            samples.append(sample)

            # Print summary
            print(f"✓ Success: {sample['result']['success']}")
            print(f"✓ Operations: {sample['result']['operations_count']}")
            print(f"✓ DAW Commands: {len(sample['result']['daw_commands'])}")

            if sample["result"]["operations"]:
                for i, op in enumerate(sample["result"]["operations"]):
                    print(
                        f"  Operation {i + 1}: {op.get('type', 'unknown')} - {op.get('description', 'N/A')}"
                    )

        except Exception as e:
            print(f"✗ Error: {e}")
            sample = {
                "timestamp": datetime.now().isoformat(),
                "scenario": scenario["name"],
                "prompt": scenario["prompt"],
                "expected_type": scenario["expected_type"],
                "error": str(e),
                "result": None,
            }
            samples.append(sample)

    # Save samples to file
    output_file = f"sample_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    with open(output_file, "w") as f:
        json.dump(samples, f, indent=2)

    print(f"\n{'=' * 60}")
    print(f"Sample data saved to: {output_file}")
    print(f"Total scenarios tested: {len(samples)}")
    print(
        f"Successful scenarios: {sum(1 for s in samples if s.get('result', {}).get('success', False))}"
    )
    print(f"{'=' * 60}")

    return samples


if __name__ == "__main__":
    capture_sample_data()
