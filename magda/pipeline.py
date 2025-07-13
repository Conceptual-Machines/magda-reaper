from typing import Dict, Any, List
from .agents.operation_identifier import OperationIdentifier
from .agents.track_agent import TrackAgent
from .agents.clip_agent import ClipAgent
from .agents.volume_agent import VolumeAgent
from .agents.effect_agent import EffectAgent
from .agents.midi_agent import MidiAgent
from .agents.base import BaseAgent
from .context_manager import ContextManager


class MAGDAPipeline:
    """Main pipeline orchestrator for MAGDA with context awareness."""
    
    def __init__(self):
        self.operation_identifier = OperationIdentifier()
        self.context_manager = ContextManager()
        self.agents: Dict[str, BaseAgent] = {
            "track": TrackAgent(),
            "clip": ClipAgent(),
            "volume": VolumeAgent(),
            "effect": EffectAgent(),
            "midi": MidiAgent(),
        }
    
    def process_prompt(self, prompt: str) -> Dict[str, Any]:
        """Process a natural language prompt through the MAGDA pipeline with context awareness."""
        
        # Stage 1: Identify operations
        print("Stage 1: Identifying operations...")
        identification_result = self.operation_identifier.execute(prompt)
        operations = identification_result["operations"]
        
        print(f"Identified {len(operations)} operations:")
        for op in operations:
            op_type = op.get('type', 'unknown')
            op_desc = op.get('description', str(op))
            print(f"  - {op_type}: {op_desc}")
        
        # Stage 2: Execute operations with specialized agents and context awareness
        print("\nStage 2: Executing operations...")
        results = []
        
        for operation in operations:
            op_type = operation["type"]
            op_description = operation["description"]
            op_parameters = operation.get("parameters", {})
            
            print(f"Processing {op_type} operation: {op_description}")
            
            # Find appropriate agent
            agent = self.agents.get(op_type)
            if not agent:
                print(f"Warning: No agent found for operation type '{op_type}'")
                continue
            
            # Execute operation with context awareness
            try:
                # Resolve track references in parameters
                resolved_parameters = self._resolve_parameters(op_parameters)
                
                # Combine operation description with resolved parameters for context
                operation_text = f"{op_description}"
                if resolved_parameters:
                    operation_text += f" with parameters: {resolved_parameters}"
                
                # Create context with resolved parameters and context manager
                agent_context = {
                    "context_manager": self.context_manager,
                    **resolved_parameters  # Include resolved parameters in context
                }
                
                # Execute with context
                result = agent.execute(operation_text, agent_context)
                results.append(result)
                
                # Update context based on operation result
                self._update_context_from_result(op_type, result, resolved_parameters)
                
                print(f"  ✓ {result['daw_command']}")
                
            except Exception as e:
                print(f"  ✗ Error executing {op_type} operation: {e}")
        
        return {
            "original_prompt": prompt,
            "operations": operations,
            "results": results,
            "daw_commands": [r["daw_command"] for r in results if "daw_command" in r],
            "context_summary": self.context_manager.get_context_summary()
        }
    
    def _resolve_parameters(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Resolve track references and other context-dependent parameters."""
        resolved = parameters.copy()
        
        # Resolve track references
        if "track_name" in resolved:
            track_ref = resolved["track_name"]
            track_obj = self.context_manager.resolve_track_reference(track_ref)
            if track_obj:
                resolved["track_id"] = track_obj.magda_id
                resolved["track_daw_id"] = track_obj.daw_id
            else:
                # Track doesn't exist yet, will be created
                resolved["track_id"] = None
        
        # Handle "current" or "this" references
        for key, value in resolved.items():
            if isinstance(value, str) and value.lower() in ["current", "this", "selected"]:
                if self.context_manager.current_selection:
                    # Use the first selected track
                    for obj_id in self.context_manager.current_selection:
                        obj = self.context_manager.objects_by_magda_id.get(obj_id)
                        if obj and obj.object_type.value == "track":
                            resolved[key] = obj.magda_id
                            break
        
        return resolved
    
    def _update_context_from_result(self, operation_type: str, result: Dict[str, Any], 
                                   parameters: Dict[str, Any]) -> None:
        """Update context based on operation result."""
        if "result" not in result:
            return
        
        operation_result = result["result"]
        
        if operation_type == "track":
            # Track creation
            track_name = parameters.get("name", f"track_{len(self.context_manager.tracks)}")
            track_type = parameters.get("track_type", "audio")
            vst = parameters.get("vst")
            
            # Create track in context manager
            track_obj = self.context_manager.create_track(
                name=track_name,
                daw_id=operation_result.get("id"),  # Use the generated ID as DAW ID
                track_type=track_type,
                vst=vst
            )
            
            # Update the result with the context manager's track ID
            operation_result["magda_id"] = track_obj.magda_id
            
        elif operation_type == "clip":
            # Clip creation
            track_id = parameters.get("track_id")
            if track_id:
                start_bar = parameters.get("start_bar", 1)
                end_bar = parameters.get("end_bar", start_bar + 4)
                
                clip_obj = self.context_manager.add_clip(
                    track_id=track_id,
                    start_bar=start_bar,
                    end_bar=end_bar,
                    daw_id=operation_result.get("id")
                )
                
                if clip_obj:
                    operation_result["magda_id"] = clip_obj.magda_id
        
        elif operation_type == "effect":
            # Effect addition
            track_id = parameters.get("track_id")
            if track_id:
                effect_type = parameters.get("effect", "reverb")
                effect_params = operation_result.get("parameters", {})
                
                effect_obj = self.context_manager.add_effect(
                    track_id=track_id,
                    effect_type=effect_type,
                    daw_id=operation_result.get("id"),
                    parameters=effect_params
                )
                
                if effect_obj:
                    operation_result["magda_id"] = effect_obj.magda_id
        
        elif operation_type == "volume":
            # Volume automation
            track_id = parameters.get("track_id")
            if track_id:
                start_value = operation_result.get("start_value", 0.0)
                end_value = operation_result.get("end_value", 1.0)
                start_bar = operation_result.get("start_bar", 1)
                end_bar = operation_result.get("end_bar", start_bar + 4)
                
                automation_obj = self.context_manager.add_volume_automation(
                    track_id=track_id,
                    start_value=start_value,
                    end_value=end_value,
                    start_bar=start_bar,
                    end_bar=end_bar,
                    daw_id=operation_result.get("id")
                )
                
                if automation_obj:
                    operation_result["magda_id"] = automation_obj.magda_id
        
        elif operation_type == "midi":
            # MIDI event
            track_id = parameters.get("track_id")
            if track_id:
                note = operation_result.get("note", "C4")
                velocity = operation_result.get("velocity", 100)
                duration = operation_result.get("duration", 1.0)
                start_bar = operation_result.get("start_bar", 1)
                
                midi_obj = self.context_manager.add_midi_event(
                    track_id=track_id,
                    note=note,
                    velocity=velocity,
                    duration=duration,
                    start_bar=start_bar,
                    daw_id=operation_result.get("id")
                )
                
                if midi_obj:
                    operation_result["magda_id"] = midi_obj.magda_id
    
    def get_agent_info(self) -> Dict[str, Any]:
        """Get information about all available agents."""
        agent_info = {}
        for name, agent in self.agents.items():
            agent_info[name] = agent.get_agent_info()
        return agent_info
    
    def get_context_summary(self) -> Dict[str, Any]:
        """Get a summary of the current context."""
        return self.context_manager.get_context_summary()
    
    def set_current_selection(self, object_ids: List[str]) -> None:
        """Set the current selection in the DAW."""
        self.context_manager.set_current_selection(object_ids)
    
    def clear_context(self) -> None:
        """Clear all context (start fresh session)."""
        self.context_manager.clear_context()
    
    def find_track_by_name(self, name: str):
        """Find a track by name."""
        return self.context_manager.find_track_by_name(name)
    
    def find_track_by_id(self, track_id: str):
        """Find a track by ID."""
        return self.context_manager.find_track_by_id(track_id) 