"""
Utility functions for MAGDA Python implementation.

This module contains utility functions for model selection, retry logic,
and other common operations.
"""

import time
from collections.abc import Callable
from typing import TypeVar

from magda.config import AgentConfig, ModelConfig

T = TypeVar("T")

# Fast-path keyword mapping for direct routing to specialized agents
OPERATION_KEYWORDS = {
    # Track operations
    "track": "track",
    "pista": "track",
    "spur": "track",
    "piste": "track",
    "channel": "track",
    "crear pista": "track",
    "add track": "track",
    "create track": "track",
    "new track": "track",
    "make track": "track",
    "nueva pista": "track",
    "nouvelle piste": "track",
    "neue spur": "track",
    "新しいトラック": "track",
    # Clip operations
    "clip": "clip",
    "fragmento": "clip",
    "fragment": "clip",
    "clip audio": "clip",
    "create clip": "clip",
    "add clip": "clip",
    "delete clip": "clip",
    "remove clip": "clip",
    "move clip": "clip",
    "crear fragmento": "clip",
    "supprimer fragment": "clip",
    "fragment löschen": "clip",
    "クリップ作成": "clip",
    # Volume operations
    "volume": "volume",
    "volumen": "volume",
    "lautstärke": "volume",
    "gain": "volume",
    "level": "volume",
    "db": "volume",
    "decibel": "volume",
    "set volume": "volume",
    "adjust volume": "volume",
    "change volume": "volume",
    "turn up": "volume",
    "turn down": "volume",
    "mute": "volume",
    "unmute": "volume",
    "silence": "volume",
    "silent": "volume",
    "silenciar": "volume",
    "réduire le volume": "volume",
    "lautstärke ändern": "volume",
    "音量設定": "volume",
    "音量": "volume",
    "音量を": "volume",
    "設定": "volume",
    # Effect operations
    "effect": "effect",
    "efecto": "effect",
    "effet": "effect",
    "effekt": "effect",
    "reverb": "effect",
    "delay": "effect",
    "compressor": "effect",
    "compression": "effect",
    "chorus": "effect",
    "flanger": "effect",
    "distortion": "effect",
    "eq": "effect",
    "equalizer": "effect",
    "filter": "effect",
    "add effect": "effect",
    "remove effect": "effect",
    "add reverb": "effect",
    "add delay": "effect",
    "add compression": "effect",
    "add chorus": "effect",
    "add eq": "effect",
    "apply effect": "effect",
    "put effect": "effect",
    "agregar efecto": "effect",
    "ajouter effet": "effect",
    "effekt hinzufügen": "effect",
    "エフェクト追加": "effect",
    # MIDI operations
    "midi": "midi",
    "note": "midi",
    "chord": "midi",
    "nota": "midi",
    "accord": "midi",
    "akord": "midi",
    "add note": "midi",
    "delete note": "midi",
    "add chord": "midi",
    "velocity": "midi",
    "agregar nota": "midi",
    "ajouter note": "midi",
    "note hinzufügen": "midi",
    "ノート追加": "midi",
}


def fast_path_route(prompt: str) -> str | None:
    """
    Fast-path routing based on keyword matching with fuzzy matching support.

    Args:
        prompt: The input prompt to analyze

    Returns:
        Operation type if unambiguous match found, None otherwise
    """
    prompt_lower = prompt.lower()
    matched_operations = set()

    # First, try exact keyword matches
    for keyword, operation_type in OPERATION_KEYWORDS.items():
        if keyword in prompt_lower:
            matched_operations.add(operation_type)

    # If no exact matches, try fuzzy matching
    if not matched_operations:
        best_match = None
        best_score = 0
        threshold = 0.8  # 80% similarity threshold

        for keyword, operation_type in OPERATION_KEYWORDS.items():
            # Calculate similarity using simple ratio
            similarity = _calculate_similarity(prompt_lower, keyword)
            if similarity > threshold and similarity > best_score:
                best_score = similarity
                best_match = operation_type

        if best_match:
            matched_operations.add(best_match)

    # If multiple matches, try to disambiguate based on specificity
    if len(matched_operations) > 1:
        # Prefer more specific keywords over general ones
        specific_keywords = {
            "add compression": "effect",
            "add reverb": "effect",
            "add delay": "effect",
            "add chorus": "effect",
            "add eq": "effect",
            "create track": "track",
            "add track": "track",
            "make track": "track",
            "set volume": "volume",
            "adjust volume": "volume",
            "turn up": "volume",
            "turn down": "volume",
            "mute": "volume",
            "silence": "volume",
        }

        # Check if any specific keyword matches
        for keyword, operation_type in specific_keywords.items():
            if keyword in prompt_lower:
                return operation_type

        # If no specific match, return None (ambiguous)
        return None

    # Return operation type if exactly one match, otherwise None
    if len(matched_operations) == 1:
        return list(matched_operations)[0]
    return None


def _calculate_similarity(text: str, keyword: str) -> float:
    """
    Calculate similarity between text and keyword using simple ratio.

    Args:
        text: The text to compare
        keyword: The keyword to match against

    Returns:
        Similarity score between 0 and 1
    """
    if not text or not keyword:
        return 0.0

    # Simple similarity calculation
    # Check if keyword is a substring
    if keyword in text:
        return 1.0

    # Check for word-level matches
    text_words = set(text.split())
    keyword_words = set(keyword.split())

    if keyword_words.intersection(text_words):
        return 0.9

    # Check for partial word matches
    for word in text_words:
        for kw in keyword_words:
            if len(kw) > 3 and (word.startswith(kw) or kw.startswith(word)):
                return 0.8

    # Check for character-level similarity
    common_chars = sum(1 for c in keyword if c in text)
    if len(keyword) > 0:
        char_similarity = common_chars / len(keyword)
        if char_similarity > 0.7:
            return char_similarity

    return 0.0


def assess_task_complexity(prompt: str) -> str:
    """
    Assess the complexity of a task based on the prompt.

    This function uses the semantic complexity detector if available,
    falling back to the simple word-count based approach.

    Args:
        prompt: The input prompt to analyze

    Returns:
        Complexity level: 'simple', 'medium', or 'complex'
    """
    try:
        # Try to use the semantic complexity detector
        from .complexity.semantic_detector import get_complexity_detector

        detector = get_complexity_detector()
        result = detector.detect_complexity(prompt)

        # Log the semantic analysis result for debugging
        print(
            f"🔍 Semantic complexity analysis: {result.complexity} (confidence: {result.confidence:.3f})"
        )
        if result.operation_type:
            print(
                f"   Operation type: {result.operation_type} (confidence: {result.operation_confidence:.3f})"
            )

        # Check reasoning requirements
        needs_reasoning = requires_reasoning(prompt)
        if needs_reasoning:
            print("🧠 Reasoning analysis: Required")
        else:
            print("🧠 Reasoning analysis: Not required")

        return result.complexity

    except Exception as e:
        # Fallback to simple word-count based approach
        print(f"⚠️  Semantic complexity detection failed, using fallback: {e}")

        # Count words
        word_count = len(prompt.split())

        # Count potential operations (rough heuristic)
        operation_indicators = [
            "create",
            "add",
            "delete",
            "remove",
            "set",
            "change",
            "move",
            "crear",
            "agregar",
            "eliminar",
            "quitar",
            "establecer",
            "cambiar",
            "mover",
            "ajouter",
            "supprimer",
            "retirer",
            "définir",
            "changer",
            "déplacer",
            "hinzufügen",
            "löschen",
            "entfernen",
            "einstellen",
            "ändern",
            "verschieben",
            "作成",
            "追加",
            "削除",
            "設定",
            "変更",
            "移動",
        ]

        operation_count = sum(
            1
            for indicator in operation_indicators
            if indicator.lower() in prompt.lower()
        )

        # Detect language (simple heuristic)
        languages = {
            "en": ["the", "and", "or", "for", "with"],
            "es": ["el", "la", "y", "o", "para", "con"],
            "fr": ["le", "la", "et", "ou", "pour", "avec"],
            "de": ["der", "die", "und", "oder", "für", "mit"],
            "ja": ["の", "と", "または", "ために", "で"],
        }

        detected_languages = []
        for lang, words in languages.items():
            if any(word in prompt.lower() for word in words):
                detected_languages.append(lang)

        # Determine complexity based on thresholds
        if (
            word_count <= ModelConfig.COMPLEXITY_THRESHOLDS["simple"]["max_words"]
            and operation_count
            <= ModelConfig.COMPLEXITY_THRESHOLDS["simple"]["max_operations"]
            and all(
                lang in ModelConfig.COMPLEXITY_THRESHOLDS["simple"]["languages"]
                for lang in detected_languages
            )
        ):
            return "simple"
        elif (
            word_count <= ModelConfig.COMPLEXITY_THRESHOLDS["medium"]["max_words"]
            and operation_count
            <= ModelConfig.COMPLEXITY_THRESHOLDS["medium"]["max_operations"]
            and all(
                lang in ModelConfig.COMPLEXITY_THRESHOLDS["medium"]["languages"]
                for lang in detected_languages
            )
        ):
            return "medium"
        else:
            return "complex"


def requires_reasoning(prompt: str) -> bool:
    """
    Determine if a prompt requires reasoning capabilities.

    Args:
        prompt: The input prompt to analyze

    Returns:
        True if the prompt requires reasoning, False otherwise
    """
    if not prompt or not prompt.strip():
        return False

    # First, check if fast-path would route this prompt
    # If fast-path can route it, no reasoning is needed
    fast_path_result = fast_path_route(prompt)
    if fast_path_result:
        return False

    prompt_lower = prompt.lower()

    # Strong reasoning indicators (these definitely require reasoning)
    strong_reasoning_indicators = [
        "if",
        "else",
        "unless",
        "when",
        "while",
        "until",
        "si",
        "sino",
        "a menos que",
        "cuando",
        "mientras",
        "hasta",
        "si",
        "sinon",
        "sauf si",
        "quand",
        "pendant",
        "jusqu",
        "wenn",
        "sonst",
        "außer",
        "während",
        "bis",
        "もし",
        "そうでなければ",
        "まで",
        "間",
    ]

    # Check for strong reasoning indicators first
    has_strong_reasoning = any(
        indicator in prompt_lower for indicator in strong_reasoning_indicators
    )
    if has_strong_reasoning:
        return True

    # Medium reasoning indicators (require context analysis)
    medium_reasoning_indicators = [
        "except",
        "without",
        "excluding",
        "excepto",
        "sin",
        "excluyendo",
        "sauf",
        "sans",
        "excluant",
        "außer",
        "ohne",
        "ausgenommen",
        "以外",
        "なしで",
    ]

    # Complex relationship indicators
    complex_indicators = [
        "all",
        "every",
        "each",
        "none",
        "some",
        "most",
        "todos",
        "cada",
        "ninguno",
        "algunos",
        "la mayoría",
        "tous",
        "chaque",
        "aucun",
        "quelques",
        "la plupart",
        "alle",
        "jeder",
        "keiner",
        "einige",
        "die meisten",
        "すべて",
        "各",
        "なし",
        "いくつか",
        "ほとんど",
    ]

    # Temporal/sequential logic
    temporal_indicators = [
        "first",
        "then",
        "after",
        "before",
        "during",
        "primero",
        "luego",
        "después",
        "antes",
        "durante",
        "d'abord",
        "puis",
        "après",
        "avant",
        "pendant",
        "zuerst",
        "dann",
        "nach",
        "vor",
        "während",
        "最初",
        "その後",
        "後",
        "前",
        "中",
    ]

    # Context-dependent operations
    context_indicators = [
        "current",
        "selected",
        "active",
        "last",
        "next",
        "actual",
        "seleccionado",
        "activo",
        "último",
        "siguiente",
        "actuel",
        "sélectionné",
        "actif",
        "dernier",
        "suivant",
        "aktuell",
        "ausgewählt",
        "aktiv",
        "letzte",
        "nächste",
        "現在",
        "選択",
        "アクティブ",
        "最後",
        "次",
    ]

    # Count reasoning indicators by category
    medium_count = sum(
        1 for indicator in medium_reasoning_indicators if indicator in prompt_lower
    )
    complex_count = sum(
        1 for indicator in complex_indicators if indicator in prompt_lower
    )
    temporal_count = sum(
        1 for indicator in temporal_indicators if indicator in prompt_lower
    )
    context_count = sum(
        1 for indicator in context_indicators if indicator in prompt_lower
    )

    # Check for complex sentence structures
    sentence_clauses = prompt.count(",") + prompt.count(" and ") + prompt.count(" or ")

    # Determine if reasoning is required
    # Only require reasoning for:
    # 1. Strong reasoning indicators (if/else, etc.)
    # 2. Multiple medium indicators + complex structure
    # 3. Very long prompts with multiple reasoning elements
    # 4. Complex temporal/contextual operations

    total_reasoning_score = (
        medium_count + complex_count + temporal_count + context_count
    )

    requires_reasoning = (
        has_strong_reasoning  # Conditional logic (if/else)
        or (
            medium_count >= 2 and sentence_clauses >= 2
        )  # Multiple medium indicators + complex structure
        or (
            total_reasoning_score >= 4 and len(prompt.split()) > 15
        )  # High reasoning score + long prompt
        or (
            temporal_count >= 2 and context_count >= 1
        )  # Multiple temporal + contextual operations
    )

    return requires_reasoning


def select_model_for_task(task_type: str, complexity: str, prompt: str = "") -> str:
    """
    Select the appropriate model based on task type, complexity, and reasoning requirements.

    Args:
        task_type: Type of task ('orchestrator' or 'specialized')
        complexity: Task complexity ('simple', 'medium', 'complex')
        prompt: The original prompt (for reasoning analysis)

    Returns:
        Model name to use
    """
    # First, check if reasoning is required
    needs_reasoning = requires_reasoning(prompt)

    if needs_reasoning:
        # Use o3-mini for reasoning tasks (expensive but necessary)
        print(f"🧠 Reasoning required, using o3-mini for {task_type}")
        return "o3-mini"

    # For non-reasoning tasks, use complexity-based selection
    if complexity == "simple":
        return ModelConfig.FAST_MODELS[task_type]
    elif complexity == "medium":
        return ModelConfig.FAST_MODELS[
            task_type
        ]  # Use fast models for medium complexity
    else:
        # For complex tasks without reasoning, use quality models
        return ModelConfig.QUALITY_MODELS[task_type]


def retry_with_backoff(
    func: Callable[..., T],
    max_retries: int = None,
    base_delay: float = None,
    backoff_factor: float = None,
    max_delay: float = None,
    *args,
    **kwargs,
) -> T:
    """
    Execute a function with exponential backoff retry logic.

    Args:
        func: Function to execute
        max_retries: Maximum number of retries (defaults to AgentConfig.MAX_RETRIES)
        base_delay: Base delay between retries (defaults to AgentConfig.RETRY_DELAY)
        backoff_factor: Backoff factor for exponential delay (defaults to AgentConfig.RETRY_BACKOFF_FACTOR)
        max_delay: Maximum delay between retries (defaults to AgentConfig.MAX_RETRY_DELAY)
        *args: Arguments to pass to func
        **kwargs: Keyword arguments to pass to func

    Returns:
        Result of func

    Raises:
        Exception: Last exception if all retries fail
    """
    max_retries = max_retries or AgentConfig.MAX_RETRIES
    base_delay = base_delay or AgentConfig.RETRY_DELAY
    backoff_factor = backoff_factor or AgentConfig.RETRY_BACKOFF_FACTOR
    max_delay = max_delay or AgentConfig.MAX_RETRY_DELAY

    last_exception = None

    for attempt in range(max_retries + 1):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            last_exception = e

            if attempt == max_retries:
                # Last attempt failed, raise the exception
                raise last_exception

            # Calculate delay with exponential backoff
            delay = min(base_delay * (backoff_factor**attempt), max_delay)

            print(f"Attempt {attempt + 1} failed: {e}. Retrying in {delay:.2f}s...")
            time.sleep(delay)

    # This should never be reached, but just in case
    raise last_exception
