import os
from typing import Any

import openai
from dotenv import load_dotenv

from magda.config import APIConfig, ModelConfig
from magda.models import IdentifiedOperation, OperationList
from magda.prompt_loader import get_prompt

from .base import BaseAgent

load_dotenv()


class OperationIdentifier(BaseAgent):
    """Agent responsible for identifying operations in natural language prompts using LLM."""

    def __init__(self) -> None:
        super().__init__()
        self.client = openai.OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

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
        """Use LLM to identify operations in the prompt using reasoning with Responses API."""

        instructions = get_prompt("operation_identifier")

        operations: list[IdentifiedOperation] = []
        try:
            response = self.client.responses.parse(
                model=ModelConfig.OPERATION_IDENTIFIER,
                instructions=instructions,
                input=prompt,
                text_format=OperationList,
                temperature=APIConfig.DEFAULT_TEMPERATURE,
            )

            # The parse method returns the parsed object directly
            if response.output_parsed is None:
                return []
            return response.output_parsed.operations
        except Exception as e:
            print(f"Error identifying operations: {e}")
        return operations

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
