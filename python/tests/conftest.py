"""
Pytest configuration for integration tests.
"""

import os
import pytest
from typing import Optional


@pytest.fixture(scope="session")
def openai_api_key() -> Optional[str]:
    """Get OpenAI API key from environment."""
    return os.getenv("OPENAI_API_KEY")


@pytest.fixture(scope="session")
def skip_if_no_api_key(openai_api_key):
    """Skip tests if no API key is available."""
    if not openai_api_key:
        pytest.skip("OPENAI_API_KEY not set - skipping integration tests")


def pytest_configure(config):
    """Configure pytest for integration tests."""
    # Add markers
    config.addinivalue_line(
        "markers", "integration: marks tests as integration tests (deselect with '-m \"not integration\"')"
    )
    config.addinivalue_line(
        "markers", "api: marks tests that make real API calls"
    )


def pytest_collection_modifyitems(config, items):
    """Automatically mark integration tests."""
    for item in items:
        if "integration" in item.nodeid:
            item.add_marker(pytest.mark.integration)
        if "test_integration" in item.nodeid:
            item.add_marker(pytest.mark.api) 