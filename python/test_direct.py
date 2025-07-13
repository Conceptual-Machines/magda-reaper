#!/usr/bin/env python3
"""
Direct test script to check API key loading and make a simple API call.
"""

import os

from dotenv import load_dotenv
from openai import OpenAI

# Load environment variables from .env file
load_dotenv("../.env")

# Force load the API key directly
api_key = os.environ.get("OPENAI_API_KEY")
if not api_key:
    # Try loading from .env file directly
    import pathlib

    env_path = pathlib.Path(__file__).parent.parent / ".env"
    if env_path.exists():
        with open(env_path) as f:
            for line in f:
                if line.startswith("OPENAI_API_KEY="):
                    api_key = line.split("=", 1)[1].strip()
                    break

print(
    f"DEBUG: Loaded API key: {api_key[:20]}..."
    if api_key
    else "DEBUG: No API key found"
)

# Make a simple API call
client = OpenAI(api_key=api_key)

try:
    print("Making API call to gpt-4o-mini...")
    response = client.responses.create(
        model="gpt-4o-mini",
        instructions="You are a helpful assistant.",
        input="Hello, how are you?",
    )

    print(f"Response type: {type(response)}")
    print(f"Response attributes: {dir(response)}")

    # Check for usage information
    if hasattr(response, "usage") and response.usage:
        print(
            f"Usage found - input: {response.usage.prompt_tokens}, output: {response.usage.completion_tokens}"
        )
    else:
        print("No usage info found in response")
        if hasattr(response, "usage"):
            print(f"Response.usage exists: {response.usage}")
        else:
            print("Response has no usage attribute")

    print("API call successful!")

except Exception as e:
    print(f"Error: {e}")
