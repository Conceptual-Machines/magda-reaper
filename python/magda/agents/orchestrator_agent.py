import os
from typing import Any

import openai
from dotenv import load_dotenv

from magda.config import APIConfig, ModelConfig
from magda.models import IdentifiedOperation, OperationList
from magda.prompt_loader import get_prompt
from magda.utils import (
    assess_task_complexity,
    retry_with_backoff,
    select_model_for_task,
)

from .base import BaseAgent

load_dotenv()


class OrchestratorAgent(BaseAgent):
    """Agent responsible for identifying operations in natural language prompts using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.client = openai.OpenAI(
            api_key=os.getenv("OPENAI_API_KEY"), timeout=APIConfig.ORCHESTRATOR_TIMEOUT
        )

    def can_handle(self, operation: str) -> bool:
        """This agent can handle any operation as it's the entry point."""
        return True

    def execute(
        self, prompt: str, context: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Parse the prompt and identify operations using LLM."""
        operations = self.identify_operations_with_llm(prompt)

        return {
            # Return Pydantic models directly
            "operations": operations,
            "original_prompt": prompt,
            "context": context or {},
        }

    def identify_operations_with_llm(self, prompt: str) -> list[IdentifiedOperation]:
        """Use LLM to identify operations in the prompt using dynamic model selection and retry logic."""

        instructions = get_prompt("orchestrator_agent")

        # Assess task complexity and select appropriate model
        complexity = assess_task_complexity(prompt)
        selected_model = select_model_for_task("orchestrator", complexity, prompt)

        print(f"Task complexity: {complexity}, using model: {selected_model}")

        def _make_api_call() -> list[IdentifiedOperation]:
            """Make the API call with retry logic."""
            # Prepare parameters based on model
            params = {
                "model": selected_model,
                "instructions": instructions,
                "input": prompt,
                "text_format": OperationList,
            }

            # Only add temperature for models that support it
            if not selected_model.startswith("o3"):
                params["temperature"] = APIConfig.DEFAULT_TEMPERATURE

            response = self.client.responses.parse(**params)

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return []
            return response.output_parsed.operations

        # Use retry logic with exponential backoff
        try:
            operations = retry_with_backoff(_make_api_call)
            return operations
        except Exception as e:
            print(f"All retries failed for orchestrator agent: {e}")
            # Fallback to a simpler model if the selected model fails
            if selected_model != ModelConfig.FALLBACK_MODELS["orchestrator"]:
                print(f"Falling back to {ModelConfig.FALLBACK_MODELS['orchestrator']}")
                try:
                    fallback_params = {
                        "model": ModelConfig.FALLBACK_MODELS["orchestrator"],
                        "instructions": instructions,
                        "input": prompt,
                        "text_format": OperationList,
                        "temperature": APIConfig.DEFAULT_TEMPERATURE,
                    }
                    response = self.client.responses.parse(**fallback_params)
                    if response.output_parsed is None:
                        return []
                    return response.output_parsed.operations
                except Exception as fallback_error:
                    print(f"Fallback model also failed: {fallback_error}")
                    return []
            return []

    def get_capabilities(self) -> list[str]:
        """Return list of operations this agent can identify."""
        return ["track", "clip", "volume", "effect", "midi"]

    def extract_operation_chain(self, prompt: str) -> list[dict[str, Any]]:
        """Extract a chain of operations from the prompt using LLM."""
        operations = self.identify_operations_with_llm(prompt)

        # Build context chain
        operation_chain = []
        current_context = {}

        for op in operations:
            if op.type == "track":
                # Track creation creates context for subsequent operations
                track_info = op.parameters
                current_context["track"] = track_info
                operation_chain.append(
                    {"operation": op, "context": current_context.copy()}
                )
            else:
                operation_chain.append(
                    {"operation": op, "context": current_context.copy()}
                )

        return operation_chain
