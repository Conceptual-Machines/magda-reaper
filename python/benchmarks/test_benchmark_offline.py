#!/usr/bin/env python3
"""
Test benchmark with offline mode enabled from the start.
"""

import os

# Set offline mode BEFORE importing anything
os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"
os.environ["HF_DATASETS_OFFLINE"] = "1"
os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"

print("🔧 Setting offline mode for Hugging Face...")
print(f"HF_HUB_OFFLINE: {os.environ.get('HF_HUB_OFFLINE')}")
print(f"TRANSFORMERS_OFFLINE: {os.environ.get('TRANSFORMERS_OFFLINE')}")
print(f"HF_DATASETS_OFFLINE: {os.environ.get('HF_DATASETS_OFFLINE')}")

# Now import and run the benchmark
if __name__ == "__main__":
    print("\n🚀 Running benchmark with offline mode...")
    print("=" * 60)

    # Import and run the benchmark
    from test_complexity_benchmark import main

    main()
