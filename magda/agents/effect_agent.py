from typing import Dict, Any, List
import uuid
from .base import BaseAgent
import openai
import os
from dotenv import load_dotenv
from ..models import EffectResult, EffectParameters, DAWCommand, AgentResponse

load_dotenv()

class EffectAgent(BaseAgent):
    """Agent responsible for handling effect operations using LLM."""
    
    def __init__(self):
        super().__init__()
        self.effects = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))
    
    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle effect operations."""
        return operation.lower() in ['effect', 'reverb', 'delay', 'compressor', 'eq', 'filter', 'distortion']
    
    def execute(self, operation: str, context: Dict[str, Any] | None = None) -> Dict[str, Any]:
        """Execute effect operation using LLM with context awareness."""
        context = context or {}
        context_manager = context.get("context_manager")
        
        # Use LLM to parse effect operation and extract parameters
        effect_info = self._parse_effect_operation_with_llm(operation)
        
        # Get track information from context
        track_id = "unknown"
        if context_manager:
            # Try to get track from context parameters
            if "track_id" in context:
                track_id = context["track_id"]
            elif "track_daw_id" in context:
                track_id = context["track_daw_id"]
        
        # Generate unique effect ID
        effect_id = str(uuid.uuid4())
        
        # Create effect result
        effect_result = EffectResult(
            id=effect_id,
            track_id=track_id,
            effect_type=effect_info.get("effect_type", "reverb"),
            parameters=effect_info.get("parameters", EffectParameters()),
            position=effect_info.get("position", "insert")
        )
        
        # Store effect for future reference
        self.effects[effect_id] = effect_result.dict()
        
        # Generate DAW command
        daw_command = self._generate_daw_command(effect_result)
        
        response = AgentResponse(
            result=effect_result.dict(),
            daw_command=daw_command,
            context=context
        )
        
        return response.dict()
    
    def _parse_effect_operation_with_llm(self, operation: str) -> Dict[str, Any]:
        """Use LLM to parse effect operation and extract parameters using Responses API."""
        
        instructions = """You are an effect specialist for a DAW system.
        Your job is to parse effect requests and extract the necessary parameters.
        
        Extract the following information:
        - effect_type: The type of effect (reverb, delay, compressor, eq, filter, distortion, etc.)
        - parameters: A dictionary of effect parameters (e.g., {"wet": 0.5, "decay": 2.0})
        - position: Where to insert the effect (insert, send, master, default: insert)
        
        Return a JSON object with the extracted parameters following the provided schema."""
        
        try:
            response = self.client.responses.parse(
                model="gpt-4.1",
                instructions=instructions,
                input=operation,
                text_format=EffectResult,
                temperature=0.1
            )
            
            # The parse method returns the parsed object directly
            return response.output_parsed.dict()
            
        except Exception as e:
            print(f"Error parsing effect operation: {e}")
            return {}
    
    def _generate_daw_command(self, effect: EffectResult) -> str:
        """Generate DAW command string from effect result."""
        params = effect.parameters
        params_str = f"wet:{params.wet}, dry:{params.dry}"
        if effect.effect_type == "compressor":
            params_str += f", threshold:{params.threshold}, ratio:{params.ratio}"
        elif effect.effect_type == "reverb":
            params_str += f", decay:{params.decay}"
        elif effect.effect_type == "delay":
            params_str += f", feedback:{params.feedback}"
        return f"effect(track:{effect.track_id}, type:{effect.effect_type}, position:{effect.position}, params:{{{params_str}}})"
    
    def get_capabilities(self) -> List[str]:
        """Return list of operations this agent can handle."""
        return ["effect", "reverb", "delay", "compressor", "eq", "filter", "distortion"] 