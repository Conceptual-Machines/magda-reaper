import os
from unittest.mock import Mock, patch

import pytest

from magda.models import AgentResponse, Operation, OperationType


@pytest.fixture(scope="session")
def mock_openai_client():
    """Mock OpenAI client for testing."""
    with patch("openai.OpenAI") as mock_client:
        yield mock_client


@pytest.fixture
def sample_operation():
    """Sample operation for testing."""
    return Operation(
        operation_type=OperationType.CREATE_TRACK,
        parameters={"track_name": "bass", "instrument": "serum"},
        agent_name="track_agent",
    )


@pytest.fixture
def sample_agent_response():
    """Sample agent response for testing."""
    return AgentResponse(
        daw_commands=["track(bass, serum)"],
        context={"tracks": {"bass": "track_1"}},
        reasoning="Created bass track with Serum",
    )


@pytest.fixture
def complex_operations():
    """Complex multi-operation setup for testing."""
    return [
        Operation(
            operation_type=OperationType.CREATE_TRACK,
            parameters={"track_name": "bass", "instrument": "serum"},
            agent_name="track_agent",
        ),
        Operation(
            operation_type=OperationType.ADD_EFFECT,
            parameters={"track_name": "bass", "effect": "compressor", "ratio": 4},
            agent_name="effect_agent",
        ),
        Operation(
            operation_type=OperationType.SET_VOLUME,
            parameters={"track_name": "bass", "volume": -6},
            agent_name="volume_agent",
        ),
    ]


@pytest.fixture
def mock_api_response():
    """Mock API response for testing."""

    def _create_mock_response(content):
        mock_response = Mock()
        mock_response.choices = [Mock()]
        mock_response.choices[0].message.content = content
        return mock_response

    return _create_mock_response


@pytest.fixture(autouse=True)
def mock_env_vars():
    """Mock environment variables for testing."""
    with patch.dict(os.environ, {"OPENAI_API_KEY": "test-api-key"}):
        yield


@pytest.fixture
def sample_prompts():
    """Sample prompts for testing different scenarios."""
    return {
        "simple_track": "create a bass track with serum",
        "multiple_tracks": "create a track for bass and one for drums",
        "complex_operation": "create a bass track with serum, add compressor with 4:1 ratio, and set volume to -6dB",
        "multilingual": {
            "spanish": "crear una pista de bajo con Serum y aplicar compresión",
            "french": "créer une piste de basse avec Serum et ajouter une compression",
            "german": "erstellen Sie eine Bass-Spur mit Serum und fügen Sie Kompression hinzu",
            "italian": "creare una traccia di basso con Serum e aggiungere compressione",
        },
        "no_operations": "this should return no operations",
        "invalid_prompt": "invalid prompt that should cause errors",
    }


@pytest.fixture
def expected_daw_commands():
    """Expected DAW commands for different scenarios."""
    return {
        "simple_track": ["track(bass, serum)"],
        "multiple_tracks": ["track(bass, serum)", "track(drums, addictive_drums)"],
        "complex_operation": [
            "track(bass, serum)",
            "effect(bass, compressor, ratio=4, threshold=-20)",
            "volume(bass, -6)",
        ],
        "volume_operation": ["volume(bass, -6)"],
        "effect_operation": ["effect(bass, compressor, ratio=4, threshold=-20)"],
        "midi_operation": ["midi(piano, quantize=16th, transpose=2)"],
    }


@pytest.fixture
def mock_context():
    """Mock context for testing."""
    return {
        "tracks": {"bass": "track_1", "drums": "track_2", "piano": "track_3"},
        "effects": {"bass": ["compressor", "reverb"], "drums": ["eq"]},
        "volumes": {"bass": -6, "drums": -3, "piano": 0},
        "clips": {"bass": "clip_1", "drums": "clip_2"},
        "midi": {"piano": "midi_1"},
    }


@pytest.fixture
def sample_effect_parameters():
    """Sample effect parameters for testing."""
    return {
        "compressor": {
            "ratio": 4.0,
            "threshold": -20.0,
            "attack": 0.01,
            "release": 0.1,
        },
        "reverb": {"wet_mix": 0.3, "dry_mix": 0.7, "decay": 2.5},
        "delay": {"delay_time": 0.5, "feedback": 0.25, "wet_mix": 0.4},
        "eq": {"frequency": 1000.0, "q_factor": 1.0, "gain": 3.0},
    }


@pytest.fixture
def mock_pipeline_result():
    """Mock pipeline result for testing."""
    return {
        "daw_commands": [
            "track(bass, serum)",
            "effect(bass, compressor, ratio=4, threshold=-20)",
            "volume(bass, -6)",
        ],
        "context": {
            "tracks": {"bass": "track_1"},
            "effects": {"bass": ["compressor"]},
            "volumes": {"bass": -6},
        },
        "reasoning": "Created bass track with Serum, added compressor, and set volume",
    }


# Markers for different test types
def pytest_configure(config):
    """Configure custom pytest markers."""
    config.addinivalue_line("markers", "integration: mark test as integration test")
    config.addinivalue_line("markers", "slow: mark test as slow running")
    config.addinivalue_line("markers", "unit: mark test as unit test")
    config.addinivalue_line("markers", "api: mark test as API test")


# Skip tests that require API calls unless explicitly requested
def pytest_collection_modifyitems(config, items):
    """Skip API tests unless --run-api flag is provided."""
    if not config.getoption("--run-api"):
        skip_api = pytest.mark.skip(reason="API tests require --run-api flag")
        for item in items:
            if "api" in item.keywords:
                item.add_marker(skip_api)


def pytest_addoption(parser):
    """Add custom command line options."""
    parser.addoption(
        "--run-api",
        action="store_true",
        default=False,
        help="run API tests that require actual OpenAI API calls",
    )
