from typing import Dict, Any, List
import uuid
from .base import BaseAgent
import openai
import os
from dotenv import load_dotenv
from ..models import ClipResult, DAWCommand, AgentResponse

load_dotenv()

class ClipAgent(BaseAgent):
    """Agent responsible for handling clip creation operations using LLM."""
    
    def __init__(self):
        super().__init__()
        self.created_clips = {}
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))
    
    def can_handle(self, operation: str) -> bool:
        """Check if this agent can handle clip operations."""
        return operation.lower() in ['clip', 'create clip', 'add clip']
    
    def execute(self, operation: str, context: Dict[str, Any] | None = None) -> Dict[str, Any]:
        """Execute clip creation operation using LLM."""
        context = context or {}
        
        # Use LLM to parse clip operation and extract parameters
        clip_info = self._parse_clip_operation_with_llm(operation)
        
        # Get track information from context
        track_id = context.get("track_id")
        if not track_id:
            # Try to get track from context
            track_context = context.get("track")
            if track_context and "id" in track_context:
                track_id = track_context["id"]
        
        # Generate unique clip ID
        clip_id = str(uuid.uuid4())
        
        # Create clip result
        clip_result = ClipResult(
            id=clip_id,
            track_id=track_id or "unknown",
            start_bar=clip_info.get("start_bar", 1),
            end_bar=clip_info.get("end_bar", clip_info.get("start_bar", 1) + 4)
        )
        
        # Store clip for future reference
        self.created_clips[clip_id] = clip_result.dict()
        
        # Generate DAW command
        daw_command = self._generate_daw_command(clip_result)
        
        response = AgentResponse(
            result=clip_result.dict(),
            daw_command=daw_command,
            context=context
        )
        
        return response.dict()
    
    def _parse_clip_operation_with_llm(self, operation: str) -> Dict[str, Any]:
        """Use LLM to parse clip operation and extract parameters using Responses API."""
        
        instructions = """You are a clip creation specialist for a DAW system.
        Your job is to parse clip creation requests and extract the necessary parameters.
        
        Extract the following information:
        - start_bar: The starting bar number (default: 1)
        - end_bar: The ending bar number (default: start_bar + 4)
        - track_reference: Any reference to a specific track
        
        Return a JSON object with the extracted parameters following the provided schema."""
        
        try:
            response = self.client.responses.parse(
                model="gpt-4.1",
                instructions=instructions,
                input=operation,
                text_format=ClipResult,
                temperature=0.1
            )
            
            # The parse method returns the parsed object directly
            return response.output_parsed.dict()
            
        except Exception as e:
            print(f"Error parsing clip operation: {e}")
            return {}
    
    def _generate_daw_command(self, clip: ClipResult) -> str:
        """Generate DAW command string from clip result."""
        return f"clip(track:{clip.track_id}, start:{clip.start_bar}, end:{clip.end_bar})"
    
    def get_capabilities(self) -> List[str]:
        """Return list of operations this agent can handle."""
        return ["clip", "create clip", "add clip"]
    
    def get_clip_by_id(self, clip_id: str) -> Dict[str, Any]:
        """Get clip information by ID."""
        return self.created_clips.get(clip_id)
    
    def list_clips(self) -> List[Dict[str, Any]]:
        """List all created clips."""
        return list(self.created_clips.values())
    
    def get_clips_for_track(self, track_id: str) -> List[Dict[str, Any]]:
        """Get all clips for a specific track."""
        return [clip for clip in self.created_clips.values() if clip["track_id"] == track_id] 