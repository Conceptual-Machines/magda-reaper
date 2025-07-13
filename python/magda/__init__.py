"""
MAGDA - Multi Agent Generative DAW API

A Python implementation of the MAGDA system for natural language DAW control.
"""

from pathlib import Path

# Calculate paths relative to this package
PACKAGE_ROOT = Path(__file__).parent
PROJECT_ROOT = PACKAGE_ROOT.parent.parent
SHARED_UTILS_PATH = PROJECT_ROOT / "shared" / "utils"
SHARED_PROMPTS_PATH = PROJECT_ROOT / "shared" / "prompts"
SHARED_SCHEMAS_PATH = PROJECT_ROOT / "shared" / "schemas"

# Version
__version__ = "0.1.0"
