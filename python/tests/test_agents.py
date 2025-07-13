from unittest.mock import patch

import pytest

from magda.agents.clip_agent import ClipAgent
from magda.agents.effect_agent import EffectAgent
from magda.agents.midi_agent import MidiAgent
from magda.agents.track_agent import TrackAgent
from magda.agents.volume_agent import VolumeAgent
from magda.models import (
    Operation,
    OperationType,
)

# Remove tests that instantiate BaseAgent directly


class TestTrackAgent:
    @pytest.fixture
    def agent(self):
        return TrackAgent()

    def test_agent_initialization(self, agent):
        assert agent.name == "track"

    def test_process_create_track(self, agent):
        # Patch the LLM parsing method to return all required fields for TrackResult
        with patch.object(agent, "_parse_track_operation_with_llm") as mock_parse:
            mock_parse.return_value = {
                "name": "bass",
                "vst": "serum",
            }
            op = Operation(
                operation_type=OperationType.CREATE_TRACK,
                parameters={"track_name": "bass", "instrument": "serum"},
                agent_name="track",
            )
            result = agent.execute(str(op.parameters), {})
            assert "name:bass" in result["daw_command"]
            assert "vst:serum" in result["daw_command"]
            assert result["daw_command"].startswith("track(")


class TestClipAgent:
    @pytest.fixture
    def agent(self):
        return ClipAgent()

    def test_agent_initialization(self, agent):
        assert agent.name == "clip"


class TestVolumeAgent:
    @pytest.fixture
    def agent(self):
        return VolumeAgent()

    def test_agent_initialization(self, agent):
        assert agent.name == "volume"


class TestEffectAgent:
    @pytest.fixture
    def agent(self):
        return EffectAgent()

    def test_agent_initialization(self, agent):
        assert agent.name == "effect"


class TestMidiAgent:
    @pytest.fixture
    def agent(self):
        return MidiAgent()

    def test_agent_initialization(self, agent):
        assert agent.name == "midi"


# Remove or skip tests that patch openai.responses.Responses.create directly or test abstract base agent
# Focus on public API and correct patching for LLM calls
