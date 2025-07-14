#!/usr/bin/env python3
"""
Debug script to test Japanese prompt processing in isolation.
"""

import os
import time
from magda.pipeline import MAGDAPipeline
from magda.utils import fast_path_route, assess_task_complexity, select_model_for_task

def test_japanese_prompt():
    """Test Japanese prompt processing step by step."""

    # Test prompt
    prompt = "トラック「ベース」の音量を-6dBに設定してください"
    print(f"Testing Japanese prompt: {prompt}")
    print(f"Prompt meaning: Please set the volume of track 'Bass' to -6dB")

    # Step 1: Test fast-path routing with detailed debug
    print("\n1. Testing fast-path routing...")
    start_time = time.time()

    # Debug: Check all keywords that match
    from magda.utils import OPERATION_KEYWORDS
    prompt_lower = prompt.lower()
    matched_keywords = []
    for keyword, operation_type in OPERATION_KEYWORDS.items():
        if keyword in prompt_lower:
            matched_keywords.append((keyword, operation_type))

    print(f"   Matched keywords: {matched_keywords}")

    fast_route = fast_path_route(prompt)
    routing_time = time.time() - start_time
    print(f"   Fast-path result: {fast_route}")
    print(f"   Routing time: {routing_time:.3f}s")

    # Step 2: Test complexity assessment
    print("\n2. Testing complexity assessment...")
    complexity = assess_task_complexity(prompt)
    print(f"   Complexity: {complexity}")

    # Step 3: Test model selection
    print("\n3. Testing model selection...")
    selected_model = select_model_for_task("specialized", complexity, prompt)
    print(f"   Selected model: {selected_model}")

    # Step 4: Test pipeline processing with timeout
    print("\n4. Testing pipeline processing...")
    pipeline = MAGDAPipeline()

    start_time = time.time()
    try:
        result = pipeline.process_prompt(prompt)
        processing_time = time.time() - start_time
        print(f"   Processing time: {processing_time:.3f}s")
        print(f"   Result: {result}")
    except Exception as e:
        processing_time = time.time() - start_time
        print(f"   Error after {processing_time:.3f}s: {e}")
        print(f"   Error type: {type(e)}")

if __name__ == "__main__":
    test_japanese_prompt()
