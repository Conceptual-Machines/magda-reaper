#!/usr/bin/env python3
"""
Script to check all available models from OpenAI API.
"""

import os

from openai import OpenAI


def check_available_models():
    """Check all available models from OpenAI API."""
    client = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    try:
        models = client.models.list()

        print("Available OpenAI Models:")
        print("=" * 50)

        # Group models by type
        gpt_models = []
        o3_models = []
        claude_models = []
        other_models = []

        for model in models.data:
            model_id = model.id

            if model_id.startswith("gpt-4"):
                gpt_models.append(model_id)
            elif model_id.startswith("o3"):
                o3_models.append(model_id)
            elif model_id.startswith("claude"):
                claude_models.append(model_id)
            else:
                other_models.append(model_id)

        print("\nGPT Models:")
        for model in sorted(gpt_models):
            print(f"  - {model}")

        print("\nO3 Models:")
        for model in sorted(o3_models):
            print(f"  - {model}")

        print("\nClaude Models:")
        for model in sorted(claude_models):
            print(f"  - {model}")

        if other_models:
            print("\nOther Models:")
            for model in sorted(other_models):
                print(f"  - {model}")

    except Exception as e:
        print(f"Error fetching models: {e}")


if __name__ == "__main__":
    if not os.getenv("OPENAI_API_KEY"):
        print("❌ Error: OPENAI_API_KEY environment variable not set")
        exit(1)

    check_available_models()
