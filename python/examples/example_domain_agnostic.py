#!/usr/bin/env python3
"""
Example demonstrating the domain-agnostic MAGDA architecture.

This example shows how to use MAGDA with different domains (DAW, Desktop, Web, etc.)
using the domain-agnostic interfaces and factory pattern.
"""

import os

from dotenv import load_dotenv

# Import domain-agnostic core
from magda.core.domain import DomainType
from magda.core.pipeline import MAGDACorePipeline

# Import DAW domain implementation
from magda.domains.daw import DAWFactory

load_dotenv()


def create_daw_pipeline() -> MAGDACorePipeline:
    """Create a DAW domain pipeline."""
    daw_factory = DAWFactory()
    return MAGDACorePipeline(daw_factory, DomainType.DAW)


def create_desktop_pipeline() -> MAGDACorePipeline:
    """Create a Desktop domain pipeline (placeholder)."""
    # This would use a DesktopFactory when implemented
    raise NotImplementedError("Desktop domain not yet implemented")


def create_web_pipeline() -> MAGDACorePipeline:
    """Create a Web domain pipeline (placeholder)."""
    # This would use a WebFactory when implemented
    raise NotImplementedError("Web domain not yet implemented")


def demonstrate_daw_domain():
    """Demonstrate DAW domain functionality."""
    print("🎵 Demonstrating DAW Domain")
    print("=" * 50)

    # Create DAW pipeline
    pipeline = create_daw_pipeline()

    # Set host context (e.g., VST plugins from Reaper)
    host_context = {
        "vst_plugins": ["serum", "addictive drums", "kontakt", "massive"],
        "track_names": ["bass", "drums", "guitar", "lead"],
        "fx_chain": ["reverb", "compression", "eq"],
        "project_settings": {"tempo": 120, "time_signature": "4/4"},
    }
    pipeline.set_host_context(host_context)

    # Test prompts
    test_prompts = [
        "create a bass track with serum",
        "add compression to guitar track",
        "set volume to -6dB on drums track",
        "create bass track with serum add compression 4:1 ratio set volume -6dB",
    ]

    for prompt in test_prompts:
        print(f"\n📝 Prompt: {prompt}")
        result = pipeline.process_prompt(prompt)

        if result.get("success", False):
            print(f"✅ Success: {result.get('commands', [])}")
        else:
            print(f"❌ Error: {result.get('error', 'Unknown error')}")

    # Show domain info
    domain_info = pipeline.get_domain_info()
    print(f"\n🏗️ Domain Info: {domain_info}")


def demonstrate_domain_switching():
    """Demonstrate switching between different domains."""
    print("\n🔄 Demonstrating Domain Switching")
    print("=" * 50)

    # Create different domain pipelines
    domains = [
        ("DAW", DomainType.DAW, create_daw_pipeline),
        # ("Desktop", DomainType.DESKTOP, create_desktop_pipeline),  # Not implemented yet
        # ("Web", DomainType.WEB, create_web_pipeline),  # Not implemented yet
    ]

    for domain_name, domain_type, pipeline_creator in domains:
        try:
            print(f"\n🎯 Testing {domain_name} Domain")
            pipeline = pipeline_creator()

            # Show domain capabilities
            domain_info = pipeline.get_domain_info()
            print(f"Supported operations: {domain_info['supported_operations']}")

            # Test with domain-appropriate prompt
            if domain_type == DomainType.DAW:
                test_prompt = "create a bass track with serum"
            elif domain_type == DomainType.DESKTOP:
                test_prompt = "open file explorer and create a new folder"
            elif domain_type == DomainType.WEB:
                test_prompt = "navigate to google.com and search for python"
            else:
                test_prompt = "test operation"

            print(f"Test prompt: {test_prompt}")
            result = pipeline.process_prompt(test_prompt)

            if result.get("success", False):
                print(f"✅ {domain_name} domain working")
            else:
                print(
                    f"❌ {domain_name} domain error: {result.get('error', 'Unknown')}"
                )

        except NotImplementedError as e:
            print(f"⏳ {domain_name} domain not yet implemented: {e}")
        except Exception as e:
            print(f"❌ {domain_name} domain error: {e}")


def demonstrate_host_integration():
    """Demonstrate host integration with context."""
    print("\n🔌 Demonstrating Host Integration")
    print("=" * 50)

    # Create DAW pipeline
    pipeline = create_daw_pipeline()

    # Simulate Reaper integration
    reaper_context = {
        "vst_plugins": ["serum", "addictive drums", "kontakt", "massive", "fabfilter"],
        "track_names": ["bass", "drums", "guitar", "lead", "synth", "piano"],
        "fx_chain": ["reverb", "compression", "eq", "delay", "chorus"],
        "custom_actions": ["custom_script_1", "custom_script_2"],
        "project_settings": {
            "tempo": 128,
            "time_signature": "4/4",
            "sample_rate": 44100,
            "bit_depth": 24,
        },
    }

    # Set host context
    pipeline.set_host_context(reaper_context)

    # Test with host-specific prompts
    host_prompts = [
        "create bass track with serum",  # Uses host VST list
        "add fabfilter compression to guitar track",  # Uses host FX list
        "set volume to -6dB on drums track",  # Uses host track names
        "create synth track with massive",  # Uses host VST list
    ]

    for prompt in host_prompts:
        print(f"\n📝 Host Prompt: {prompt}")
        result = pipeline.process_prompt(prompt)

        if result.get("success", False):
            print("✅ Host integration working")
            print(f"Commands: {result.get('commands', [])}")
        else:
            print(f"❌ Host integration error: {result.get('error', 'Unknown')}")


def main():
    """Main demonstration function."""
    print("🚀 MAGDA Domain-Agnostic Architecture Demo")
    print("=" * 60)

    # Check for OpenAI API key
    if not os.getenv("OPENAI_API_KEY"):
        print("⚠️  Warning: OPENAI_API_KEY not set. Some features may not work.")
        print(
            "   Set your OpenAI API key in the .env file to test with real LLM calls."
        )

    try:
        # Demonstrate DAW domain
        demonstrate_daw_domain()

        # Demonstrate domain switching
        demonstrate_domain_switching()

        # Demonstrate host integration
        demonstrate_host_integration()

        print("\n🎉 Domain-agnostic architecture demonstration complete!")
        print("\n📋 Key Benefits:")
        print("   ✅ Domain-agnostic core interfaces")
        print("   ✅ Easy to add new domains")
        print("   ✅ Host context integration")
        print("   ✅ Consistent API across domains")
        print("   ✅ Factory pattern for domain creation")

    except Exception as e:
        print(f"❌ Demo error: {e}")


if __name__ == "__main__":
    main()
