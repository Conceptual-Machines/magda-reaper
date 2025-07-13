import logging
from pathlib import Path

PROMPT_DIR = Path(__file__).parent.parent.parent / "shared" / "prompts"


def get_prompt(prompt_name: str) -> str:
    """
    Load a prompt from shared/prompts/{prompt_name}.md.
    Returns the prompt as a string. Logs a warning and returns a minimal fallback if not found.
    """
    prompt_path = PROMPT_DIR / f"{prompt_name}.md"
    if prompt_path.exists():
        return prompt_path.read_text(encoding="utf-8")
    logging.warning(f"Prompt file not found: {prompt_path}. Using fallback prompt.")
    return f"You are a helpful assistant. (Prompt file '{prompt_name}.md' missing.)"
