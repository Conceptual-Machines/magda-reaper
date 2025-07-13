"""
Context Manager for MAGDA - maintains state and ID mappings between MAGDA and the DAW.

This module provides context awareness by tracking:
- Track mappings (MAGDA ID ↔ DAW ID)
- Clip references and their locations
- Effect chains and parameters
- Current selection state
- Volume automation data
"""

from typing import Any, Dict, List, Optional, Union
from enum import Enum
import uuid
from dataclasses import dataclass, field


class ObjectType(Enum):
    """Types of objects that can be tracked in context."""
    TRACK = "track"
    CLIP = "clip"
    EFFECT = "effect"
    AUTOMATION = "automation"


@dataclass
class TrackObject:
    """Represents a track in the context manager."""
    magda_id: str
    daw_id: Optional[str]
    name: str
    track_type: str = "audio"  # audio, midi, bus, etc.
    vst: Optional[str] = None
    clips: List[str] = field(default_factory=list)  # List of clip MAGDA IDs
    effects: List[str] = field(default_factory=list)  # List of effect MAGDA IDs
    automations: List[str] = field(default_factory=list)  # List of automation MAGDA IDs


@dataclass
class ClipObject:
    """Represents a clip in the context manager."""
    magda_id: str
    daw_id: Optional[str]
    track_id: str  # MAGDA ID of the track
    start_bar: int
    end_bar: int
    name: Optional[str] = None


@dataclass
class EffectObject:
    """Represents an effect in the context manager."""
    magda_id: str
    daw_id: Optional[str]
    track_id: str  # MAGDA ID of the track
    effect_type: str
    parameters: Dict[str, Any] = field(default_factory=dict)


@dataclass
class AutomationObject:
    """Represents volume automation in the context manager."""
    magda_id: str
    daw_id: Optional[str]
    track_id: str  # MAGDA ID of the track
    start_value: float
    end_value: float
    start_bar: int
    end_bar: int


class ContextManager:
    """
    Manages context and state between MAGDA and the DAW.
    
    Provides:
    - ID mapping between MAGDA and DAW objects
    - Track reference resolution
    - Context-aware parameter resolution
    - State persistence across operations
    """
    
    def __init__(self) -> None:
        """Initialize the context manager with empty state."""
        self.tracks: Dict[str, TrackObject] = {}
        self.clips: Dict[str, ClipObject] = {}
        self.effects: Dict[str, EffectObject] = {}
        self.automations: Dict[str, AutomationObject] = {}
        
        # Index for quick lookups
        self.objects_by_magda_id: Dict[str, Union[TrackObject, ClipObject, EffectObject, AutomationObject]] = {}
        self.tracks_by_name: Dict[str, TrackObject] = {}
        
        # Current selection state (from DAW)
        self.current_selection: List[str] = []
        
        # Counter for generating unique IDs
        self._id_counter = 0
    
    def _generate_magda_id(self, prefix: str = "obj") -> str:
        """Generate a unique MAGDA ID."""
        self._id_counter += 1
        return f"{prefix}_{self._id_counter}_{uuid.uuid4().hex[:8]}"
    
    def create_track(self, name: str, daw_id: Optional[str] = None, 
                    track_type: str = "audio", vst: Optional[str] = None) -> TrackObject:
        """
        Create a new track in the context manager.
        
        Args:
            name: Track name
            daw_id: DAW's internal track ID (if known)
            track_type: Type of track (audio, midi, bus, etc.)
            vst: Associated VST plugin (if any)
            
        Returns:
            TrackObject representing the created track
        """
        magda_id = self._generate_magda_id("track")
        
        track_obj = TrackObject(
            magda_id=magda_id,
            daw_id=daw_id,
            name=name,
            track_type=track_type,
            vst=vst
        )
        
        self.tracks[magda_id] = track_obj
        self.objects_by_magda_id[magda_id] = track_obj
        self.tracks_by_name[name.lower()] = track_obj
        
        return track_obj
    
    def add_clip(self, track_id: str, start_bar: int, end_bar: int, 
                daw_id: Optional[str] = None, name: Optional[str] = None) -> Optional[ClipObject]:
        """
        Add a clip to a track.
        
        Args:
            track_id: MAGDA ID of the track
            start_bar: Starting bar position
            end_bar: Ending bar position
            daw_id: DAW's internal clip ID (if known)
            name: Clip name (optional)
            
        Returns:
            ClipObject if successful, None if track not found
        """
        if track_id not in self.tracks:
            return None
        
        magda_id = self._generate_magda_id("clip")
        
        clip_obj = ClipObject(
            magda_id=magda_id,
            daw_id=daw_id,
            track_id=track_id,
            start_bar=start_bar,
            end_bar=end_bar,
            name=name
        )
        
        self.clips[magda_id] = clip_obj
        self.objects_by_magda_id[magda_id] = clip_obj
        
        # Add to track's clip list
        self.tracks[track_id].clips.append(magda_id)
        
        return clip_obj
    
    def add_effect(self, track_id: str, effect_type: str, 
                  daw_id: Optional[str] = None, parameters: Optional[Dict[str, Any]] = None) -> Optional[EffectObject]:
        """
        Add an effect to a track.
        
        Args:
            track_id: MAGDA ID of the track
            effect_type: Type of effect (reverb, delay, etc.)
            daw_id: DAW's internal effect ID (if known)
            parameters: Effect parameters
            
        Returns:
            EffectObject if successful, None if track not found
        """
        if track_id not in self.tracks:
            return None
        
        magda_id = self._generate_magda_id("effect")
        
        effect_obj = EffectObject(
            magda_id=magda_id,
            daw_id=daw_id,
            track_id=track_id,
            effect_type=effect_type,
            parameters=parameters or {}
        )
        
        self.effects[magda_id] = effect_obj
        self.objects_by_magda_id[magda_id] = effect_obj
        
        # Add to track's effect list
        self.tracks[track_id].effects.append(magda_id)
        
        return effect_obj
    
    def add_volume_automation(self, track_id: str, start_value: float, end_value: float,
                            start_bar: int, end_bar: int, daw_id: Optional[str] = None) -> Optional[AutomationObject]:
        """
        Add volume automation to a track.
        
        Args:
            track_id: MAGDA ID of the track
            start_value: Starting volume value
            end_value: Ending volume value
            start_bar: Starting bar position
            end_bar: Ending bar position
            daw_id: DAW's internal automation ID (if known)
            
        Returns:
            AutomationObject if successful, None if track not found
        """
        if track_id not in self.tracks:
            return None
        
        magda_id = self._generate_magda_id("automation")
        
        automation_obj = AutomationObject(
            magda_id=magda_id,
            daw_id=daw_id,
            track_id=track_id,
            start_value=start_value,
            end_value=end_value,
            start_bar=start_bar,
            end_bar=end_bar
        )
        
        self.automations[magda_id] = automation_obj
        self.objects_by_magda_id[magda_id] = automation_obj
        
        # Add to track's automation list
        self.tracks[track_id].automations.append(magda_id)
        
        return automation_obj
    
    def add_midi_event(self, track_id: str, note: str, velocity: int, duration: float,
                      start_bar: int, daw_id: Optional[str] = None) -> Optional[Any]:
        """
        Add MIDI event to a track.
        
        Args:
            track_id: MAGDA ID of the track
            note: MIDI note (e.g., "C4")
            velocity: Note velocity (0-127)
            duration: Note duration in beats
            start_bar: Starting bar position
            daw_id: DAW's internal MIDI ID (if known)
            
        Returns:
            MIDI object if successful, None if track not found
        """
        if track_id not in self.tracks:
            return None
        
        # For now, just return a simple object since MIDI events aren't fully implemented
        return {
            "magda_id": self._generate_magda_id("midi"),
            "daw_id": daw_id,
            "track_id": track_id,
            "note": note,
            "velocity": velocity,
            "duration": duration,
            "start_bar": start_bar
        }
    
    def resolve_track_reference(self, reference: str) -> Optional[TrackObject]:
        """
        Resolve a track reference to a TrackObject.
        
        Args:
            reference: Track name, ID, or special reference ("current", "this", etc.)
            
        Returns:
            TrackObject if found, None otherwise
        """
        # Direct MAGDA ID lookup
        if reference in self.tracks:
            return self.tracks[reference]
        
        # Name lookup (case-insensitive)
        reference_lower = reference.lower()
        if reference_lower in self.tracks_by_name:
            return self.tracks_by_name[reference_lower]
        
        # Special references
        if reference_lower in ["current", "this", "selected"]:
            if self.current_selection:
                # Return the first selected track
                for obj_id in self.current_selection:
                    obj = self.objects_by_magda_id.get(obj_id)
                    if obj and isinstance(obj, TrackObject):
                        return obj
        
        # Try partial name matching
        for track in self.tracks.values():
            if reference_lower in track.name.lower():
                return track
        
        return None
    
    def find_track_by_name(self, name: str) -> Optional[TrackObject]:
        """Find a track by name (case-insensitive)."""
        return self.tracks_by_name.get(name.lower())
    
    def find_track_by_id(self, track_id: str) -> Optional[TrackObject]:
        """Find a track by MAGDA ID or DAW ID."""
        # First try MAGDA ID
        if track_id in self.tracks:
            return self.tracks[track_id]
        
        # Then try DAW ID
        for track in self.tracks.values():
            if track.daw_id == track_id:
                return track
        
        return None
    
    def get_track_clips(self, track_id: str) -> List[ClipObject]:
        """Get all clips for a track."""
        if track_id not in self.tracks:
            return []
        
        return [self.clips[clip_id] for clip_id in self.tracks[track_id].clips 
                if clip_id in self.clips]
    
    def get_track_effects(self, track_id: str) -> List[EffectObject]:
        """Get all effects for a track."""
        if track_id not in self.tracks:
            return []
        
        return [self.effects[effect_id] for effect_id in self.tracks[track_id].effects 
                if effect_id in self.effects]
    
    def get_track_automations(self, track_id: str) -> List[AutomationObject]:
        """Get all automations for a track."""
        if track_id not in self.tracks:
            return []
        
        return [self.automations[auto_id] for auto_id in self.tracks[track_id].automations 
                if auto_id in self.automations]
    
    def set_current_selection(self, object_ids: List[str]) -> None:
        """Set the current selection from the DAW."""
        self.current_selection = object_ids
    
    def get_current_track(self) -> Optional[TrackObject]:
        """Get the currently selected track."""
        if not self.current_selection:
            return None
        
        for obj_id in self.current_selection:
            obj = self.objects_by_magda_id.get(obj_id)
            if obj and isinstance(obj, TrackObject):
                return obj
        
        return None
    
    def get_context_summary(self) -> Dict[str, Any]:
        """Get a summary of the current context."""
        total_objects = len(self.tracks) + len(self.clips) + len(self.effects) + len(self.automations)
        
        return {
            "tracks": {
                track_id: {
                    "name": track.name,
                    "daw_id": track.daw_id,
                    "type": track.track_type,
                    "vst": track.vst,
                    "effects": len(track.effects),
                    "volume_automations": len(track.automations),
                    "midi_events": 0,  # Placeholder for future MIDI support
                    "clips_count": len(track.clips),
                    "effects_count": len(track.effects),
                    "automations_count": len(track.automations)
                }
                for track_id, track in self.tracks.items()
            },
            "clips_count": len(self.clips),
            "effects_count": len(self.effects),
            "automations_count": len(self.automations),
            "total_objects": total_objects,
            "current_selection": self.current_selection
        }
    
    def clear_context(self) -> None:
        """Clear all context data."""
        self.tracks.clear()
        self.clips.clear()
        self.effects.clear()
        self.automations.clear()
        self.objects_by_magda_id.clear()
        self.tracks_by_name.clear()
        self.current_selection.clear()
        self._id_counter = 0 