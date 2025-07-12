from pydantic import BaseModel, Field
from typing import Dict, Any, List, Optional
from enum import Enum


class OperationType(str, Enum):
    """Enumeration of available operation types."""
    TRACK = "track"
    CLIP = "clip"
    VOLUME = "volume"
    EFFECT = "effect"
    MIDI = "midi"


class OperationParameters(BaseModel):
    """Base model for operation parameters."""
    pass


class TrackParameters(OperationParameters):
    """Parameters for track creation operations."""
    vst: Optional[str] = Field(None, description="VST plugin name")
    name: Optional[str] = Field(None, description="Track name")
    type: Optional[str] = Field(None, description="Track type (audio, midi, etc.)")


class ClipParameters(OperationParameters):
    """Parameters for clip creation operations."""
    start_bar: Optional[int] = Field(None, description="Starting bar number")
    end_bar: Optional[int] = Field(None, description="Ending bar number")
    track_id: Optional[str] = Field(None, description="Target track ID")
    track_name: Optional[str] = Field(None, description="Target track name")


class VolumeParameters(OperationParameters):
    """Parameters for volume automation operations."""
    start_value: Optional[float] = Field(None, description="Starting volume value")
    end_value: Optional[float] = Field(None, description="Ending volume value")
    start_bar: Optional[int] = Field(None, description="Starting bar number")
    end_bar: Optional[int] = Field(None, description="Ending bar number")


class EffectParameters(OperationParameters):
    """Parameters for effect operations."""
    effect_type: Optional[str] = Field(None, description="Type of effect")
    parameters: Optional[Dict[str, Any]] = Field(None, description="Effect parameters")
    track_id: Optional[str] = Field(None, description="Target track ID")


class MidiParameters(OperationParameters):
    """Parameters for MIDI operations."""
    note: Optional[str] = Field(None, description="MIDI note")
    velocity: Optional[int] = Field(None, description="Note velocity")
    duration: Optional[float] = Field(None, description="Note duration")
    track_id: Optional[str] = Field(None, description="Target track ID")


class Operation(BaseModel):
    """Model for a single operation."""
    type: OperationType = Field(..., description="Type of operation")
    description: str = Field(..., description="Human-readable description of the operation")
    parameters: Dict[str, Any] = Field(default_factory=dict, description="Operation parameters")


class OperationList(BaseModel):
    """Model for a list of operations."""
    operations: List[Operation] = Field(..., description="List of identified operations")


class TrackResult(BaseModel):
    """Model for track creation result."""
    id: str = Field(..., description="Unique track ID")
    name: str = Field(..., description="Track name")
    vst: str = Field("", description="VST plugin (empty string if none)")
    type: str = Field("track", description="Track type")
    
    class Config:
        json_schema_extra = {
            "additionalProperties": False
        }


class ClipResult(BaseModel):
    """Model for clip creation result."""
    id: str = Field(..., description="Unique clip ID")
    track_id: str = Field(..., description="Target track ID")
    start_bar: int = Field(..., description="Starting bar")
    end_bar: int = Field(..., description="Ending bar")
    
    class Config:
        extra = "forbid"  # This is equivalent to additionalProperties: false


class VolumeResult(BaseModel):
    """Model for volume automation result."""
    id: str = Field(..., description="Unique volume automation ID")
    track_id: str = Field(..., description="Target track ID")
    start_value: float = Field(..., description="Starting volume value (0.0 to 1.0)")
    end_value: float = Field(..., description="Ending volume value (0.0 to 1.0)")
    start_bar: int = Field(..., description="Starting bar")
    end_bar: int = Field(..., description="Ending bar")
    
    class Config:
        extra = "forbid"  # This is equivalent to additionalProperties: false


class EffectResult(BaseModel):
    """Model for effect result."""
    id: str = Field(..., description="Unique effect ID")
    track_id: str = Field(..., description="Target track ID")
    effect_type: str = Field(..., description="Type of effect (reverb, delay, compressor, etc.)")
    parameters: Dict[str, Any] = Field(..., description="Effect parameters")
    position: str = Field(..., description="Effect position (insert, send, master)")
    
    class Config:
        extra = "forbid"  # This is equivalent to additionalProperties: false


class MidiResult(BaseModel):
    """Model for MIDI result."""
    id: str = Field(..., description="Unique MIDI event ID")
    track_id: str = Field(..., description="Target track ID")
    note: str = Field(..., description="MIDI note (e.g., C4, F#3)")
    velocity: int = Field(..., description="Note velocity (0-127)")
    duration: float = Field(..., description="Note duration in beats")
    start_bar: int = Field(..., description="Starting bar")
    channel: int = Field(..., description="MIDI channel (1-16)")
    
    class Config:
        extra = "forbid"  # This is equivalent to additionalProperties: false


class DAWCommand(BaseModel):
    """Model for DAW command output."""
    command: str = Field(..., description="The DAW command string")
    parameters: Dict[str, Any] = Field(..., description="Command parameters")
    result_id: Optional[str] = Field(None, description="ID of the created object")


class AgentResponse(BaseModel):
    """Model for agent response."""
    result: Dict[str, Any] = Field(..., description="Operation result")
    daw_command: str = Field(..., description="Generated DAW command")
    context: Dict[str, Any] = Field(default_factory=dict, description="Operation context") 