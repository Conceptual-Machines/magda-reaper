"""
Context Manager for MAGDA - maintains state and ID mappings between MAGDA and DAW.
"""

from typing import Dict, Any, List, Optional, Set
import uuid
from dataclasses import dataclass, field
from enum import Enum


class ObjectType(Enum):
    """Types of DAW objects that can be tracked."""
    TRACK = "track"
    CLIP = "clip"
    EFFECT = "effect"
    VOLUME_AUTOMATION = "volume_automation"
    MIDI_EVENT = "midi_event"


@dataclass
class DAWObject:
    """Represents a DAW object with both MAGDA and DAW IDs."""
    magda_id: str
    daw_id: Optional[str] = None
    object_type: ObjectType = ObjectType.TRACK
    name: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=lambda: __import__('time').time())
    
    def __post_init__(self):
        if not self.magda_id:
            self.magda_id = str(uuid.uuid4())


@dataclass
class TrackContext:
    """Context for a specific track."""
    track: DAWObject
    clips: List[DAWObject] = field(default_factory=list)
    effects: List[DAWObject] = field(default_factory=list)
    volume_automations: List[DAWObject] = field(default_factory=list)
    midi_events: List[DAWObject] = field(default_factory=list)
    
    def add_clip(self, clip: DAWObject) -> None:
        """Add a clip to this track."""
        self.clips.append(clip)
    
    def add_effect(self, effect: DAWObject) -> None:
        """Add an effect to this track."""
        self.effects.append(effect)
    
    def add_volume_automation(self, automation: DAWObject) -> None:
        """Add volume automation to this track."""
        self.volume_automations.append(automation)
    
    def add_midi_event(self, midi_event: DAWObject) -> None:
        """Add a MIDI event to this track."""
        self.midi_events.append(midi_event)


class ContextManager:
    """Manages context and ID mappings between MAGDA and DAW."""
    
    def __init__(self):
        self.tracks: Dict[str, TrackContext] = {}
        self.objects_by_magda_id: Dict[str, DAWObject] = {}
        self.objects_by_daw_id: Dict[str, DAWObject] = {}
        self.objects_by_name: Dict[str, List[DAWObject]] = {}
        self.current_selection: Set[str] = set()
        self.session_id: str = str(uuid.uuid4())
        
    def create_track(self, name: str, daw_id: Optional[str] = None, 
                    track_type: str = "audio", vst: Optional[str] = None) -> DAWObject:
        """Create a new track and add it to context."""
        magda_id = str(uuid.uuid4())
        track_obj = DAWObject(
            magda_id=magda_id,
            daw_id=daw_id,
            object_type=ObjectType.TRACK,
            name=name,
            metadata={
                "type": track_type,
                "vst": vst
            }
        )
        
        # Register the track
        self.objects_by_magda_id[magda_id] = track_obj
        if daw_id:
            self.objects_by_daw_id[daw_id] = track_obj
        
        # Add to name index
        if name not in self.objects_by_name:
            self.objects_by_name[name] = []
        self.objects_by_name[name].append(track_obj)
        
        # Create track context
        self.tracks[magda_id] = TrackContext(track=track_obj)
        
        return track_obj
    
    def find_track_by_name(self, name: str) -> Optional[DAWObject]:
        """Find a track by name (case-insensitive)."""
        name_lower = name.lower()
        for obj_name, objects in self.objects_by_name.items():
            if obj_name.lower() == name_lower:
                for obj in objects:
                    if obj.object_type == ObjectType.TRACK:
                        return obj
        return None
    
    def find_track_by_id(self, track_id: str) -> Optional[DAWObject]:
        """Find a track by MAGDA ID or DAW ID."""
        # Try MAGDA ID first
        if track_id in self.objects_by_magda_id:
            obj = self.objects_by_magda_id[track_id]
            if obj.object_type == ObjectType.TRACK:
                return obj
        
        # Try DAW ID
        if track_id in self.objects_by_daw_id:
            obj = self.objects_by_daw_id[track_id]
            if obj.object_type == ObjectType.TRACK:
                return obj
        
        return None
    
    def get_track_context(self, track_id: str) -> Optional[TrackContext]:
        """Get the context for a specific track."""
        track = self.find_track_by_id(track_id)
        if track:
            return self.tracks.get(track.magda_id)
        return None
    
    def add_clip(self, track_id: str, start_bar: int, end_bar: int, 
                daw_id: Optional[str] = None) -> Optional[DAWObject]:
        """Add a clip to a track."""
        track_context = self.get_track_context(track_id)
        if not track_context:
            return None
        
        clip_obj = DAWObject(
            magda_id=str(uuid.uuid4()),
            daw_id=daw_id,
            object_type=ObjectType.CLIP,
            name=f"clip_{start_bar}_{end_bar}",
            metadata={
                "start_bar": start_bar,
                "end_bar": end_bar,
                "track_id": track_context.track.magda_id
            }
        )
        
        # Register the clip
        self.objects_by_magda_id[clip_obj.magda_id] = clip_obj
        if daw_id:
            self.objects_by_daw_id[daw_id] = clip_obj
        
        # Add to track context
        track_context.add_clip(clip_obj)
        
        return clip_obj
    
    def add_effect(self, track_id: str, effect_type: str, daw_id: Optional[str] = None,
                  parameters: Optional[Dict[str, Any]] = None) -> Optional[DAWObject]:
        """Add an effect to a track."""
        track_context = self.get_track_context(track_id)
        if not track_context:
            return None
        
        effect_obj = DAWObject(
            magda_id=str(uuid.uuid4()),
            daw_id=daw_id,
            object_type=ObjectType.EFFECT,
            name=f"{effect_type}_effect",
            metadata={
                "effect_type": effect_type,
                "parameters": parameters or {},
                "track_id": track_context.track.magda_id
            }
        )
        
        # Register the effect
        self.objects_by_magda_id[effect_obj.magda_id] = effect_obj
        if daw_id:
            self.objects_by_daw_id[daw_id] = effect_obj
        
        # Add to track context
        track_context.add_effect(effect_obj)
        
        return effect_obj
    
    def add_volume_automation(self, track_id: str, start_value: float, end_value: float,
                            start_bar: int, end_bar: int, daw_id: Optional[str] = None) -> Optional[DAWObject]:
        """Add volume automation to a track."""
        track_context = self.get_track_context(track_id)
        if not track_context:
            return None
        
        automation_obj = DAWObject(
            magda_id=str(uuid.uuid4()),
            daw_id=daw_id,
            object_type=ObjectType.VOLUME_AUTOMATION,
            name=f"volume_automation_{start_bar}_{end_bar}",
            metadata={
                "start_value": start_value,
                "end_value": end_value,
                "start_bar": start_bar,
                "end_bar": end_bar,
                "track_id": track_context.track.magda_id
            }
        )
        
        # Register the automation
        self.objects_by_magda_id[automation_obj.magda_id] = automation_obj
        if daw_id:
            self.objects_by_daw_id[daw_id] = automation_obj
        
        # Add to track context
        track_context.add_volume_automation(automation_obj)
        
        return automation_obj
    
    def add_midi_event(self, track_id: str, note: str, velocity: int, duration: float,
                      start_bar: int, daw_id: Optional[str] = None) -> Optional[DAWObject]:
        """Add a MIDI event to a track."""
        track_context = self.get_track_context(track_id)
        if not track_context:
            return None
        
        midi_obj = DAWObject(
            magda_id=str(uuid.uuid4()),
            daw_id=daw_id,
            object_type=ObjectType.MIDI_EVENT,
            name=f"midi_{note}_{start_bar}",
            metadata={
                "note": note,
                "velocity": velocity,
                "duration": duration,
                "start_bar": start_bar,
                "track_id": track_context.track.magda_id
            }
        )
        
        # Register the MIDI event
        self.objects_by_magda_id[midi_obj.magda_id] = midi_obj
        if daw_id:
            self.objects_by_daw_id[daw_id] = midi_obj
        
        # Add to track context
        track_context.add_midi_event(midi_obj)
        
        return midi_obj
    
    def update_daw_id(self, magda_id: str, daw_id: str) -> bool:
        """Update the DAW ID for a MAGDA object."""
        if magda_id in self.objects_by_magda_id:
            obj = self.objects_by_magda_id[magda_id]
            obj.daw_id = daw_id
            self.objects_by_daw_id[daw_id] = obj
            return True
        return False
    
    def get_context_summary(self) -> Dict[str, Any]:
        """Get a summary of the current context."""
        return {
            "session_id": self.session_id,
            "tracks": {
                track_id: {
                    "name": context.track.name,
                    "daw_id": context.track.daw_id,
                    "clips": len(context.clips),
                    "effects": len(context.effects),
                    "volume_automations": len(context.volume_automations),
                    "midi_events": len(context.midi_events)
                }
                for track_id, context in self.tracks.items()
            },
            "total_objects": len(self.objects_by_magda_id),
            "current_selection": list(self.current_selection)
        }
    
    def resolve_track_reference(self, reference: str) -> Optional[DAWObject]:
        """Resolve a track reference (name, ID, or "current")."""
        # Try as exact name
        track = self.find_track_by_name(reference)
        if track:
            return track
        
        # Try as ID
        track = self.find_track_by_id(reference)
        if track:
            return track
        
        # Try "current" or "selected"
        if reference.lower() in ["current", "selected", "this"]:
            if self.current_selection:
                # Return the first selected track
                for obj_id in self.current_selection:
                    obj = self.objects_by_magda_id.get(obj_id)
                    if obj and obj.object_type == ObjectType.TRACK:
                        return obj
        
        return None
    
    def set_current_selection(self, object_ids: List[str]) -> None:
        """Set the current selection."""
        self.current_selection = set(object_ids)
    
    def clear_context(self) -> None:
        """Clear all context (start fresh session)."""
        self.tracks.clear()
        self.objects_by_magda_id.clear()
        self.objects_by_daw_id.clear()
        self.objects_by_name.clear()
        self.current_selection.clear()
        self.session_id = str(uuid.uuid4()) 