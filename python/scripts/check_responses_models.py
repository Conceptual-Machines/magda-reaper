#!/usr/bin/env python3
"""
Script to check which models support the Responses API.
"""

import os

from dotenv import load_dotenv
from openai import OpenAI

# Load environment variables
load_dotenv()


def check_responses_models():
    """Check which models support the Responses API."""
    client = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    # Get all available models
    try:
        models = client.models.list()
        print(f"Found {len(models.data)} total models")
    except Exception as e:
        print(f"Error fetching models: {e}")
        return

    # Filter for models that likely support Responses API
    # Based on the models we've seen working, these are the main ones
    responses_candidates = [
        # GPT-4.1 models
        "gpt-4.1",
        "gpt-4.1-mini",
        "gpt-4.1-nano",
        # GPT-4o models
        "gpt-4o",
        "gpt-4o-mini",
        # O3 models
        "o3-mini",
        # O1 models
        "o1",
        "o1-mini",
        "o1-pro",
        # O4 models
        "o4-mini",
    ]

    print("\nTesting Responses API support for candidate models...")
    print("=" * 60)

    supported_models = []
    unsupported_models = []

    for model_id in responses_candidates:
        print(f"Testing {model_id}...", end=" ")

        try:
            # Try a simple Responses API call
            response = client.responses.create(
                input="Hello", instructions="Say hello back", model=model_id
            )

            if response and hasattr(response, "output"):
                print("✅ SUPPORTED")
                supported_models.append(model_id)
            else:
                print("❌ Not supported")
                unsupported_models.append(model_id)

        except Exception as e:
            error_msg = str(e)
            if "not supported" in error_msg.lower() or "model_not_found" in error_msg:
                print("❌ Not supported")
                unsupported_models.append(model_id)
            else:
                print(f"❌ Error: {error_msg[:50]}...")
                unsupported_models.append(model_id)

    print("\n" + "=" * 60)
    print("RESULTS SUMMARY")
    print("=" * 60)

    print(f"\n✅ SUPPORTED MODELS ({len(supported_models)}):")
    for model in supported_models:
        print(f"  - {model}")

    print(f"\n❌ UNSUPPORTED MODELS ({len(unsupported_models)}):")
    for model in unsupported_models:
        print(f"  - {model}")

    print(f"\n📊 Total tested: {len(responses_candidates)}")
    print(f"📊 Supported: {len(supported_models)}")
    print(f"📊 Unsupported: {len(unsupported_models)}")

    return supported_models


if __name__ == "__main__":
    if not os.getenv("OPENAI_API_KEY"):
        print("❌ Error: OPENAI_API_KEY environment variable not set")
        exit(1)

    supported = check_responses_models()

    if supported:
        print("\n💡 You can use these models in your benchmark:")
        print(f"   --models {' '.join(supported)}")
