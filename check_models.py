#!/usr/bin/env python3
"""
Script to check available OpenAI models using the OpenAI SDK.
"""

import os

import openai
from dotenv import load_dotenv


def check_available_models():
    """Check and display all available OpenAI models."""
    load_dotenv()

    # Initialize OpenAI client
    client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

    try:
        # Get all models
        models = client.models.list()

        print("Available OpenAI Models:")
        print("=" * 50)

        # Group models by type
        model_groups = {
            "O3 Series": [],
            "GPT-4.1 Series": [],
            "GPT-4o Series": [],
            "GPT-4.5 Series": [],
            "GPT-3.5 Series": [],
            "Other": [],
        }

        for model in models.data:
            model_id = model.id

            # Categorize models
            if model_id.startswith("o3"):
                model_groups["O3 Series"].append(model_id)
            elif model_id.startswith("gpt-4.1"):
                model_groups["GPT-4.1 Series"].append(model_id)
            elif model_id.startswith("gpt-4o"):
                model_groups["GPT-4o Series"].append(model_id)
            elif model_id.startswith("gpt-4.5"):
                model_groups["GPT-4.5 Series"].append(model_id)
            elif model_id.startswith("gpt-3.5"):
                model_groups["GPT-3.5 Series"].append(model_id)
            else:
                model_groups["Other"].append(model_id)

        # Print grouped models
        for group_name, models_list in model_groups.items():
            if models_list:
                print(f"\n{group_name}:")
                for model_id in sorted(models_list):
                    print(f"  - {model_id}")

        # Check specific models we're interested in
        print("\n" + "=" * 50)
        print("Models of Interest for MAGDA:")

        interesting_models = [
            "o3",
            "o3-mini",
            "gpt-4.1",
            "gpt-4.1-mini",
            "gpt-4.1-nano",
            "gpt-4o",
            "gpt-4o-mini",
        ]

        available_interesting = []
        for model_id in interesting_models:
            if any(model_id in model.id for model in models.data):
                available_interesting.append(model_id)
                print(f"  ✓ {model_id} - AVAILABLE")
            else:
                print(f"  ✗ {model_id} - NOT AVAILABLE")

        return available_interesting

    except Exception as e:
        print(f"Error checking models: {e}")
        return []


if __name__ == "__main__":
    check_available_models()
