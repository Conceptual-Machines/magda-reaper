from typing import Dict, Any, List
from .agents.operation_identifier import OperationIdentifier
from .agents.track_agent import TrackAgent
from .agents.clip_agent import ClipAgent
from .agents.volume_agent import VolumeAgent
from .agents.effect_agent import EffectAgent
from .agents.midi_agent import MidiAgent
from .agents.base import BaseAgent


class MAGDAPipeline:
    """Main pipeline orchestrator for MAGDA."""
    
    def __init__(self):
        self.operation_identifier = OperationIdentifier()
        self.agents: Dict[str, BaseAgent] = {
            "track": TrackAgent(),
            "clip": ClipAgent(),
            "volume": VolumeAgent(),
            "effect": EffectAgent(),
            "midi": MidiAgent(),
        }
    
    def process_prompt(self, prompt: str) -> Dict[str, Any]:
        """Process a natural language prompt through the MAGDA pipeline."""
        
        # Stage 1: Identify operations
        print("Stage 1: Identifying operations...")
        identification_result = self.operation_identifier.execute(prompt)
        operations = identification_result["operations"]
        
        print(f"Identified {len(operations)} operations:")
        for op in operations:
            op_type = op.get('type', 'unknown')
            op_desc = op.get('description', str(op))
            print(f"  - {op_type}: {op_desc}")
        
        # Stage 2: Execute operations with specialized agents
        print("\nStage 2: Executing operations...")
        results = []
        current_context = {}
        
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
            
            # Execute operation
            try:
                # Combine operation description with parameters for context
                operation_text = f"{op_description}"
                if op_parameters:
                    operation_text += f" with parameters: {op_parameters}"
                
                result = agent.execute(operation_text, current_context)
                results.append(result)
                
                # Update context for next operations
                if op_type == "track" and "result" in result:
                    track_result = result["result"]
                    current_context["track_id"] = track_result.get("id")
                    current_context["track"] = track_result
                
                print(f"  ✓ {result['daw_command']}")
                
            except Exception as e:
                print(f"  ✗ Error executing {op_type} operation: {e}")
        
        return {
            "original_prompt": prompt,
            "operations": operations,
            "results": results,
            "daw_commands": [r["daw_command"] for r in results if "daw_command" in r]
        }
    
    def get_agent_info(self) -> Dict[str, Any]:
        """Get information about all available agents."""
        agent_info = {}
        for name, agent in self.agents.items():
            agent_info[name] = agent.get_agent_info()
        return agent_info 