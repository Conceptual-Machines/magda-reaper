#!/usr/bin/env python3
"""
Test script to verify offline loading of sentence-transformers model.
"""

import os
import time
from pathlib import Path

# Set offline mode before importing anything
os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"
os.environ["HF_DATASETS_OFFLINE"] = "1"

print("🔧 Testing offline model loading...")
print("=" * 50)

# Check if model exists locally
cache_dir = Path.home() / ".cache" / "huggingface" / "hub"
model_dir = cache_dir / "models--sentence-transformers--all-MiniLM-L6-v2"

print(f"Cache directory: {cache_dir}")
print(f"Model directory: {model_dir}")
print(f"Model exists: {model_dir.exists()}")

if model_dir.exists():
    print("✅ Model found in local cache")

    # List model files
    snapshots_dir = model_dir / "snapshots"
    if snapshots_dir.exists():
        snapshots = list(snapshots_dir.iterdir())
        if snapshots:
            latest_snapshot = snapshots[0]
            print(f"Latest snapshot: {latest_snapshot.name}")

            # List files in snapshot
            files = list(latest_snapshot.iterdir())
            print(f"Model files: {len(files)}")
            for file in files:
                if file.is_file():
                    print(f"  - {file.name}")
else:
    print("❌ Model not found in local cache")

print("\n🧠 Testing model loading...")
print("-" * 30)

try:
    start_time = time.perf_counter()

    from sentence_transformers import SentenceTransformer

    print("Loading model...")
    model = SentenceTransformer("all-MiniLM-L6-v2")

    end_time = time.perf_counter()
    load_time = (end_time - start_time) * 1000

    print(f"✅ Model loaded successfully in {load_time:.2f}ms")

    # Test a simple embedding
    test_text = "create a track called bass"
    print(f"\nTesting embedding for: '{test_text}'")

    start_time = time.perf_counter()
    embedding = model.encode(test_text)
    end_time = time.perf_counter()
    embed_time = (end_time - start_time) * 1000

    print(f"✅ Embedding created in {embed_time:.2f}ms")
    print(f"   Embedding shape: {embedding.shape}")

except Exception as e:
    print(f"❌ Error loading model: {e}")
    print(f"   Error type: {type(e).__name__}")

print("\n" + "=" * 50)
print("✅ Offline loading test completed!")
