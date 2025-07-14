#!/usr/bin/env python3
"""
Basic usage example for MAGDA library.

This example demonstrates how to use MAGDA to translate natural language
prompts into domain-specific commands.
"""

import sys
from pathlib import Path

# Add the parent directory to the path so we can import magda
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))

from magda import DAWFactory, MAGDAPipeline


def main():
    """Demonstrate basic MAGDA usage."""
    print("🎵 MAGDA - Multi Agent Domain Automation")
    print("=" * 50)

    # Initialize the pipeline
    print("\n🔄 Initializing MAGDA pipeline...")
    pipeline = MAGDAPipeline()

    # Example prompts to test
    test_prompts = [
        "Create a new track called 'Bass'",
        "Set the volume of track 'Guitar' to -6dB",
        "Add reverb to the drums track",
        "Create a 4-bar clip with a C major chord",
        "Mute all tracks except the vocals",
    ]

    print(f"\n🧪 Testing {len(test_prompts)} example prompts:")
    print("-" * 50)

    for i, prompt in enumerate(test_prompts, 1):
        print(f"\n{i}. Prompt: '{prompt}'")

        try:
            # Process the prompt
            result = pipeline.process_prompt(prompt)

            # Display results
            print("   ✅ Success!")
            print(f"   📝 Commands: {len(result.get('commands', []))}")

            if result.get("commands"):
                for j, command in enumerate(result["commands"], 1):
                    print(f"      {j}. {command}")

            if result.get("daw_commands"):
                print(f"   🎵 DAW Commands: {len(result['daw_commands'])}")
                for j, command in enumerate(result["daw_commands"], 1):
                    print(f"      {j}. {command}")

        except Exception as e:
            print(f"   ❌ Error: {e}")

    print("\n" + "=" * 50)
    print("🎉 Demo completed!")

    # Show how to use the domain-agnostic features
    print("\n🔧 Domain-Agnostic Features:")
    print("-" * 30)

    # Create a DAW factory
    daw_factory = DAWFactory()

    # Get domain context
    context = daw_factory.create_context()
    print(f"   📊 Domain Context: {type(context).__name__}")

    # Get orchestrator
    orchestrator = daw_factory.create_orchestrator()
    print(f"   🎼 Orchestrator: {type(orchestrator).__name__}")

    # Get agents
    agents = daw_factory.create_agents()
    print(f"   🤖 Agents: {len(agents)} agents created")
    for name, agent in agents.items():
        print(f"      - {name}: {type(agent).__name__}")


if __name__ == "__main__":
    main()
