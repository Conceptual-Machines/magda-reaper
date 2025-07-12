from enum import Enum
from typing import Any

from pydantic import BaseModel, Field


class OperationType(str, Enum):
    """Enumeration of available operation types."""
    CREATE_TRACK = "CREATE_TRACK"
    MODIFY_TRACK = "MODIFY_TRACK"
    CREATE_CLIP = "CREATE_CLIP"
    MODIFY_CLIP = "MODIFY_CLIP"
    SET_VOLUME = "SET_VOLUME"
    CREATE_FADE = "CREATE_FADE"
    ADD_EFFECT = "ADD_EFFECT"
    REMOVE_EFFECT = "REMOVE_EFFECT"
    QUANTIZE_MIDI = "QUANTIZE_MIDI"
    TRANSPOSE_MIDI = "TRANSPOSE_MIDI"
    MODIFY_MIDI = "MODIFY_MIDI"


class Operation(BaseModel):
    """Model for a single operation."""
    operation_type: OperationType = Field(..., description="Type of operation")
    parameters: dict[str, Any] = Field(default_factory=dict, description="Operation parameters")
    agent_name: str = Field(..., description="Name of the agent to handle this operation")


class AgentResponse(BaseModel):
    """Base model for agent responses."""
    daw_commands: list[str] = Field(..., description="List of generated DAW commands")
    context: dict[str, Any] = Field(default_factory=dict, description="Operation context")
    reasoning: str = Field(..., description="Reasoning behind the operations")


class TrackResult(AgentResponse):
    """Model for track operation result."""
    track_id: str | None = Field(None, description="Unique track ID")
    track_name: str | None = Field(None, description="Track name")
    instrument: str | None = Field(None, description="Instrument/VST plugin")


class ClipResult(AgentResponse):
    """Model for clip operation result."""
    clip_id: str | None = Field(None, description="Unique clip ID")
    track_name: str | None = Field(None, description="Target track name")
    start_time: float | None = Field(None, description="Start time in seconds")
    duration: float | None = Field(None, description="Clip duration in seconds")


class VolumeResult(AgentResponse):
    """Model for volume operation result."""
    track_name: str | None = Field(None, description="Target track name")
    volume: float | None = Field(None, description="Volume level in dB")
    fade_type: str | None = Field(None, description="Type of fade (in, out)")
    fade_duration: float | None = Field(None, description="Fade duration in seconds")


class EffectParameters(BaseModel):
    """Model for effect parameters."""
    wet_mix: float = Field(0.5, ge=0.0, le=1.0, description="Wet mix amount (0.0 to 1.0)")
    dry_mix: float = Field(0.5, ge=0.0, le=1.0, description="Dry mix amount (0.0 to 1.0)")
    threshold: float = Field(-24.0, ge=-60.0, le=0.0, description="Compressor threshold in dB")
    ratio: float = Field(2.0, ge=1.0, le=20.0, description="Compressor ratio")
    attack: float = Field(0.01, ge=0.001, le=1.0, description="Attack time in seconds")
    release: float = Field(0.1, ge=0.001, le=5.0, description="Release time in seconds")
    decay: float = Field(2.0, ge=0.1, le=10.0, description="Reverb decay time in seconds")
    feedback: float = Field(0.25, ge=0.0, le=0.99, description="Delay feedback amount (0.0 to 0.99)")
    delay_time: float = Field(0.5, ge=0.001, le=5.0, description="Delay time in seconds")
    frequency: float = Field(1000.0, ge=20.0, le=20000.0, description="EQ frequency in Hz")
    q_factor: float = Field(1.0, ge=0.1, le=10.0, description="EQ Q factor")
    gain: float = Field(0.0, ge=-24.0, le=24.0, description="EQ gain in dB")


class EffectResult(AgentResponse):
    """Model for effect operation result."""
    track_name: str | None = Field(None, description="Target track name")
    effect_type: str | None = Field(None, description="Type of effect")
    parameters: EffectParameters | None = Field(None, description="Effect parameters")


class MIDIResult(AgentResponse):
    """Model for MIDI operation result."""
    track_name: str | None = Field(None, description="Target track name")
    operation: str | None = Field(None, description="MIDI operation type")
    quantization: str | None = Field(None, description="Quantization value")
    transpose_semitones: int | None = Field(None, description="Transpose amount in semitones")
    velocity: int | None = Field(None, ge=0, le=127, description="MIDI velocity")


# Legacy models for backward compatibility
class OperationParameters(BaseModel):
    """Base model for operation parameters."""
    pass


class TrackParameters(OperationParameters):
    """Parameters for track creation operations."""
    vst: str | None = Field(None, description="VST plugin name")
    name: str | None = Field(None, description="Track name")
    type: str | None = Field(None, description="Track type (audio, midi, etc.)")


class ClipParameters(OperationParameters):
    """Parameters for clip creation operations."""
    start_bar: int | None = Field(None, description="Starting bar number")
    end_bar: int | None = Field(None, description="Ending bar number")
    track_id: str | None = Field(None, description="Target track ID")
    track_name: str | None = Field(None, description="Target track name")


class VolumeParameters(OperationParameters):
    """Parameters for volume automation operations."""
    start_value: float | None = Field(None, description="Starting volume value")
    end_value: float | None = Field(None, description="Ending volume value")
    start_bar: int | None = Field(None, description="Starting bar number")
    end_bar: int | None = Field(None, description="Ending bar number")


class MidiParameters(OperationParameters):
    """Parameters for MIDI operations."""
    note: str | None = Field(None, description="MIDI note")
    velocity: int | None = Field(None, description="Note velocity")
    duration: float | None = Field(None, description="Note duration")
    track_id: str | None = Field(None, description="Target track ID")


class OperationList(BaseModel):
    """Model for a list of operations."""
    operations: list[Operation] = Field(..., description="List of identified operations")


class DAWCommand(BaseModel):
    """Model for DAW command output."""
    command: str = Field(..., description="The DAW command string")
    parameters: dict[str, Any] = Field(..., description="Command parameters")
    result_id: str | None = Field(None, description="ID of the created object")
