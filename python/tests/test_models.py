import pytest
from pydantic import ValidationError
from magda.models import (
    Operation, OperationType, AgentResponse, TrackResult, ClipResult,
    VolumeResult, EffectResult, EffectParameters, MIDIResult
)


class TestOperation:
    """Test cases for the Operation model."""

    def test_valid_operation(self):
        """Test creating a valid operation."""
        operation = Operation(
            operation_type=OperationType.CREATE_TRACK,
            parameters={"track_name": "bass", "instrument": "serum"},
            agent_name="track_agent"
        )
        
        assert operation.operation_type == OperationType.CREATE_TRACK
        assert operation.parameters["track_name"] == "bass"
        assert operation.parameters["instrument"] == "serum"
        assert operation.agent_name == "track_agent"

    def test_operation_with_empty_parameters(self):
        """Test operation with empty parameters."""
        operation = Operation(
            operation_type=OperationType.SET_VOLUME,
            parameters={},
            agent_name="volume_agent"
        )
        
        assert operation.parameters == {}

    def test_operation_type_enum(self):
        """Test all operation types."""
        for op_type in OperationType:
            operation = Operation(
                operation_type=op_type,
                parameters={"test": "value"},
                agent_name="test_agent"
            )
            assert operation.operation_type == op_type

    def test_invalid_operation_type(self):
        """Test that invalid operation type raises error."""
        with pytest.raises(ValidationError):
            Operation(
                operation_type="INVALID_TYPE",
                parameters={},
                agent_name="test_agent"
            )


class TestAgentResponse:
    """Test cases for the AgentResponse model."""

    def test_valid_agent_response(self):
        """Test creating a valid agent response."""
        response = AgentResponse(
            daw_commands=["track(bass, serum)", "volume(bass, -6)"],
            context={"tracks": {"bass": "track_1"}},
            reasoning="Created bass track and set volume"
        )
        
        assert len(response.daw_commands) == 2
        assert "track(bass, serum)" in response.daw_commands
        assert response.context["tracks"]["bass"] == "track_1"
        assert response.reasoning == "Created bass track and set volume"

    def test_agent_response_with_empty_commands(self):
        """Test agent response with empty commands."""
        response = AgentResponse(
            daw_commands=[],
            context={},
            reasoning="No operations needed"
        )
        
        assert response.daw_commands == []
        assert response.context == {}

    def test_agent_response_with_complex_context(self):
        """Test agent response with complex context."""
        complex_context = {
            "tracks": {"bass": "track_1", "drums": "track_2"},
            "effects": {"bass": ["compressor", "reverb"]},
            "volumes": {"bass": -6, "drums": -3}
        }
        
        response = AgentResponse(
            daw_commands=["track(bass, serum)", "track(drums, addictive_drums)"],
            context=complex_context,
            reasoning="Created multiple tracks with effects"
        )
        
        assert response.context["tracks"]["bass"] == "track_1"
        assert response.context["effects"]["bass"] == ["compressor", "reverb"]
        assert response.context["volumes"]["bass"] == -6


class TestTrackResult:
    """Test cases for the TrackResult model."""

    def test_valid_track_result(self):
        """Test creating a valid track result."""
        result = TrackResult(
            daw_commands=["track(bass, serum)"],
            context={"tracks": {"bass": "track_1"}},
            reasoning="Created bass track with Serum",
            track_id="track_1",
            track_name="bass",
            instrument="serum"
        )
        
        assert result.track_id == "track_1"
        assert result.track_name == "bass"
        assert result.instrument == "serum"

    def test_track_result_without_instrument(self):
        """Test track result without instrument specification."""
        result = TrackResult(
            daw_commands=["track(bass)"],
            context={"tracks": {"bass": "track_1"}},
            reasoning="Created bass track",
            track_id="track_1",
            track_name="bass"
        )
        
        assert result.instrument is None


class TestClipResult:
    """Test cases for the ClipResult model."""

    def test_valid_clip_result(self):
        """Test creating a valid clip result."""
        result = ClipResult(
            daw_commands=["clip(bass, 0, 4)"],
            context={"clips": {"bass": "clip_1"}},
            reasoning="Created 4-second clip on bass track",
            clip_id="clip_1",
            track_name="bass",
            start_time=0.0,
            duration=4.0
        )
        
        assert result.clip_id == "clip_1"
        assert result.track_name == "bass"
        assert result.start_time == 0.0
        assert result.duration == 4.0


class TestVolumeResult:
    """Test cases for the VolumeResult model."""

    def test_valid_volume_result(self):
        """Test creating a valid volume result."""
        result = VolumeResult(
            daw_commands=["volume(bass, -6)"],
            context={"volumes": {"bass": -6}},
            reasoning="Set bass track volume to -6dB",
            track_name="bass",
            volume=-6.0
        )
        
        assert result.track_name == "bass"
        assert result.volume == -6.0

    def test_volume_result_with_fade(self):
        """Test volume result with fade parameters."""
        result = VolumeResult(
            daw_commands=["fade(bass, 0, 2, -6)"],
            context={"volumes": {"bass": -6}},
            reasoning="Created fade-in on bass track",
            track_name="bass",
            volume=-6.0,
            fade_type="in",
            fade_duration=2.0
        )
        
        assert result.fade_type == "in"
        assert result.fade_duration == 2.0


class TestEffectParameters:
    """Test cases for the EffectParameters model."""

    def test_valid_effect_parameters(self):
        """Test creating valid effect parameters."""
        params = EffectParameters(
            wet_mix=0.3,
            dry_mix=0.7,
            threshold=-20.0,
            ratio=4.0,
            attack=0.01,
            release=0.1,
            decay=2.5,
            feedback=0.25,
            delay_time=0.5,
            frequency=1000.0,
            q_factor=1.0,
            gain=0.0
        )
        
        assert params.wet_mix == 0.3
        assert params.threshold == -20.0
        assert params.ratio == 4.0
        assert params.decay == 2.5
        assert params.feedback == 0.25

    def test_effect_parameters_with_defaults(self):
        """Test effect parameters with default values."""
        params = EffectParameters()
        
        assert params.wet_mix == 0.5
        assert params.dry_mix == 0.5
        assert params.threshold == -24.0
        assert params.ratio == 2.0

    def test_effect_parameters_validation(self):
        """Test effect parameters validation."""
        # Test wet_mix range validation
        with pytest.raises(ValidationError):
            EffectParameters(wet_mix=1.5)  # Should be <= 1.0
        
        # Test ratio range validation
        with pytest.raises(ValidationError):
            EffectParameters(ratio=0.5)  # Should be >= 1.0
        
        # Test threshold range validation
        with pytest.raises(ValidationError):
            EffectParameters(threshold=-100.0)  # Should be >= -60.0


class TestEffectResult:
    """Test cases for the EffectResult model."""

    def test_valid_effect_result(self):
        """Test creating a valid effect result."""
        params = EffectParameters(ratio=4.0, threshold=-20.0)
        result = EffectResult(
            daw_commands=["effect(bass, compressor, ratio=4, threshold=-20)"],
            context={"effects": {"bass": ["compressor"]}},
            reasoning="Added compressor to bass track",
            track_name="bass",
            effect_type="compressor",
            parameters=params
        )
        
        assert result.track_name == "bass"
        assert result.effect_type == "compressor"
        assert result.parameters.ratio == 4.0
        assert result.parameters.threshold == -20.0

    def test_effect_result_without_parameters(self):
        """Test effect result without specific parameters."""
        result = EffectResult(
            daw_commands=["effect(bass, reverb)"],
            context={"effects": {"bass": ["reverb"]}},
            reasoning="Added reverb to bass track",
            track_name="bass",
            effect_type="reverb"
        )
        
        assert result.parameters is None


class TestMIDIResult:
    """Test cases for the MIDIResult model."""

    def test_valid_midi_result(self):
        """Test creating a valid MIDI result."""
        result = MIDIResult(
            daw_commands=["midi(piano, quantize=16th, transpose=2)"],
            context={"midi": {"piano": "midi_1"}},
            reasoning="Quantized and transposed piano MIDI",
            track_name="piano",
            operation="quantize",
            quantization="16th",
            transpose_semitones=2
        )
        
        assert result.track_name == "piano"
        assert result.operation == "quantize"
        assert result.quantization == "16th"
        assert result.transpose_semitones == 2

    def test_midi_result_with_velocity(self):
        """Test MIDI result with velocity changes."""
        result = MIDIResult(
            daw_commands=["midi(piano, velocity=80)"],
            context={"midi": {"piano": "midi_1"}},
            reasoning="Set piano MIDI velocity to 80",
            track_name="piano",
            operation="velocity",
            velocity=80
        )
        
        assert result.velocity == 80
        assert result.operation == "velocity"


class TestModelSerialization:
    """Test cases for model serialization."""

    def test_operation_serialization(self):
        """Test operation model serialization."""
        operation = Operation(
            operation_type=OperationType.CREATE_TRACK,
            parameters={"track_name": "bass"},
            agent_name="track_agent"
        )
        
        data = operation.model_dump()
        assert data["operation_type"] == "CREATE_TRACK"
        assert data["parameters"]["track_name"] == "bass"
        assert data["agent_name"] == "track_agent"

    def test_agent_response_serialization(self):
        """Test agent response model serialization."""
        response = AgentResponse(
            daw_commands=["track(bass, serum)"],
            context={"tracks": {"bass": "track_1"}},
            reasoning="Created bass track"
        )
        
        data = response.model_dump()
        assert data["daw_commands"] == ["track(bass, serum)"]
        assert data["context"]["tracks"]["bass"] == "track_1"
        assert data["reasoning"] == "Created bass track"

    def test_effect_parameters_serialization(self):
        """Test effect parameters model serialization."""
        params = EffectParameters(ratio=4.0, threshold=-20.0)
        
        data = params.model_dump()
        assert data["ratio"] == 4.0
        assert data["threshold"] == -20.0
        assert data["wet_mix"] == 0.5  # default value 