"""
Semantic Complexity Detector using Sentence Transformers

This module provides a more sophisticated approach to complexity detection
using semantic similarity with pre-trained sentence transformers.
"""

import logging
import os
from dataclasses import dataclass
from typing import Any

import numpy as np

# Set offline mode BEFORE importing sentence-transformers
os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"
os.environ["HF_DATASETS_OFFLINE"] = "1"
os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"

try:
    from sentence_transformers import SentenceTransformer, util

    SENTENCE_TRANSFORMERS_AVAILABLE = True
except ImportError:
    SENTENCE_TRANSFORMERS_AVAILABLE = False
    logging.warning(
        "sentence-transformers not available. Falling back to simple detector."
    )

logger = logging.getLogger(__name__)


@dataclass
class ComplexityResult:
    """Result of complexity analysis"""

    complexity: str  # 'simple', 'medium', 'complex'
    confidence: float  # 0.0 to 1.0
    operation_type: str | None = None  # 'track', 'volume', 'effect', 'clip', 'midi'
    operation_confidence: float | None = None


class SemanticComplexityDetector:
    """
    Advanced complexity detector using sentence transformers for semantic analysis.

    This detector provides more accurate complexity classification by:
    1. Using semantic similarity instead of word counting
    2. Classifying operation types (track, volume, effect, etc.)
    3. Supporting multilingual prompts
    4. Providing confidence scores for decisions
    """

    # Class-level cache for the model to avoid reloading
    _model_cache = {}
    _embeddings_cache = {}

    def __init__(self, model_name: str = "all-MiniLM-L6-v2"):
        """
        Initialize the semantic complexity detector.

        Args:
            model_name: Name of the sentence transformer model to use
        """
        self.model_name = model_name

        # Use cached model if available
        if model_name in self._model_cache:
            self.model = self._model_cache[model_name]
            logger.info(f"Using cached model: {model_name}")
        else:
            self.model = None
            self._load_model()
            # Cache the model for future use
            if self.model is not None:
                self._model_cache[model_name] = self.model

        # Example prompts for each complexity level (with realistic typos and sloppy formatting)
        self.complexity_examples = {
            "simple": [
                "set volume to -6db",
                "create new track called guitar",
                "mute drums",
                "add reverb to bass track",
                "delete old drums track",
                "pan left",
                "set tempo 120",
                "turn up the volume",
                "make it louder",
                "silence the track",
                "create a track",
                "add a new track",
            ],
            "medium": [
                "create track and add reverb",
                "mute drums and set bass volume -3db",
                "add compression to guitar",
                "create 4 bar clip with c major chord",
                "set volume -6db and pan left",
                "add reverb 30% wet",
                "create track set volume",
                "add delay to the track",
                "compress the bass",
                "make a new track with reverb",
                "adjust volume and pan",
                "add chorus effect",
            ],
            "complex": [
                "create track add reverb set volume -3db export mix",
                "for all tracks except drums add compressor threshold -10db",
                "create bass track with serum add compression 4:1 ratio set volume -6db pan left",
                "if guitar track exists add reverb 30% wet otherwise create it first",
                "create track add multiple effects set automation export",
                "apply compression all tracks adjust volumes create master bus",
                "make a new track but only if there isnt one already",
                "add reverb to everything except the drums",
                "create a track with serum and add compression and set the volume",
                "if the track exists then add reverb otherwise make a new one",
            ],
        }

        # Example prompts for operation classification (with realistic typos and sloppy formatting)
        self.operation_examples = {
            "track": [
                "create new track called lead guitar",
                "delete old drums track",
                "rename bass track to electric bass",
                "duplicate guitar track",
                "make a new track",
                "add track",
                "remove track",
                "copy track",
            ],
            "volume": [
                "set bass track volume -6db",
                "adjust volume 75%",
                "increase volume 3db",
                "set master volume -1db",
                "turn up volume",
                "make it louder",
                "reduce volume",
                "mute track",
            ],
            "effect": [
                "add reverb to guitar track",
                "add compression 4:1 ratio",
                "apply delay 500ms",
                "add chorus effect",
                "put reverb on it",
                "compress the track",
                "add delay",
                "put chorus on",
            ],
            "clip": [
                "create 4 bar clip on drums track",
                "move clip to bar 12",
                "duplicate clip",
                "split clip at bar 8",
                "make a clip",
                "copy clip",
                "move the clip",
                "cut the clip",
            ],
            "midi": [
                "add c major chord bar 1",
                "quantize midi notes 16th notes",
                "transpose melody up 2 semitones",
                "add drum pattern",
                "put a chord there",
                "fix the timing",
                "move notes up",
                "add drums",
            ],
        }

        self._load_model()

    def _load_model(self) -> None:
        """Load the sentence transformer model."""
        if not SENTENCE_TRANSFORMERS_AVAILABLE:
            logger.warning(
                "sentence-transformers not available. Using fallback detector."
            )
            return

        try:
            logger.info(f"Loading sentence transformer model: {self.model_name}")

            # Check if model is available locally first
            import os
            from pathlib import Path

            # Set environment variables to use local cache and avoid HTTP requests
            os.environ["HF_HUB_OFFLINE"] = "1"  # Force offline mode
            os.environ["TRANSFORMERS_OFFLINE"] = "1"  # Force transformers offline
            os.environ["HF_DATASETS_OFFLINE"] = "1"  # Force datasets offline

            # Check if model files exist locally
            cache_dir = Path.home() / ".cache" / "huggingface" / "hub"
            model_dir = cache_dir / f"models--sentence-transformers--{self.model_name}"

            if model_dir.exists():
                logger.info(f"Model found in local cache: {model_dir}")
            else:
                logger.warning(f"Model not found in local cache: {model_dir}")
                logger.warning(
                    "Will attempt to load from Hugging Face (may cause rate limiting)"
                )
                # Remove offline flags to allow download
                os.environ.pop("HF_HUB_OFFLINE", None)
                os.environ.pop("TRANSFORMERS_OFFLINE", None)
                os.environ.pop("HF_DATASETS_OFFLINE", None)

            # Add timeout and retry logic for model loading
            import time

            max_retries = 3
            retry_delay = 2

            for attempt in range(max_retries):
                try:
                    self.model = SentenceTransformer(self.model_name)
                    logger.info("Model loaded successfully from local cache!")
                    return
                except Exception as e:
                    if "429" in str(e) or "rate limit" in str(e).lower():
                        if attempt < max_retries - 1:
                            logger.warning(
                                f"Rate limit hit, retrying in {retry_delay}s (attempt {attempt + 1}/{max_retries})"
                            )
                            time.sleep(retry_delay)
                            retry_delay *= 2
                            continue
                        else:
                            logger.error(
                                f"Rate limit exceeded after {max_retries} attempts. Using fallback detector."
                            )
                            self.model = None
                            return
                    else:
                        logger.error(f"Failed to load sentence transformer model: {e}")
                        self.model = None
                        return

        except Exception as e:
            logger.error(f"Failed to load sentence transformer model: {e}")
            self.model = None

    def _compute_embeddings(self, texts: list[str]) -> np.ndarray:
        """Compute embeddings for a list of texts."""
        if self.model is None:
            raise RuntimeError("Model not loaded")

        # Check cache first
        cache_key = tuple(sorted(texts))
        if cache_key in self._embeddings_cache:
            return self._embeddings_cache[cache_key]

        embeddings = self.model.encode(texts, convert_to_tensor=True)
        self._embeddings_cache[cache_key] = embeddings
        return embeddings

    def _find_best_match(
        self, query: str, examples: dict[str, list[str]]
    ) -> tuple[str, float]:
        """
        Find the best matching category for a query using semantic similarity.

        Args:
            query: The input text to classify
            examples: Dictionary mapping categories to example texts

        Returns:
            Tuple of (best_category, confidence_score)
        """
        if self.model is None:
            # Fallback: return first category with 0.5 confidence
            return list(examples.keys())[0], 0.5

        # Prepare all texts for embedding
        all_texts = [query]
        category_mapping = []

        for category, texts in examples.items():
            all_texts.extend(texts)
            category_mapping.extend([category] * len(texts))

        # Compute embeddings
        embeddings = self._compute_embeddings(all_texts)
        query_embedding = embeddings[0:1]
        example_embeddings = embeddings[1:]

        # Calculate similarities
        similarities = util.pytorch_cos_sim(query_embedding, example_embeddings)[0]

        # Find best match
        best_idx = similarities.argmax().item()
        best_similarity = similarities[best_idx].item()
        best_category = category_mapping[best_idx]

        return best_category, best_similarity

    def detect_complexity(self, prompt: str) -> ComplexityResult:
        """
        Detect the complexity of a DAW command prompt.

        Args:
            prompt: The input prompt to analyze

        Returns:
            ComplexityResult with complexity level and confidence
        """
        if not prompt or not prompt.strip():
            return ComplexityResult("simple", 0.1)

        # Clean the prompt
        prompt = prompt.strip()

        # Check for obviously invalid inputs
        if len(prompt) < 3 or prompt.isdigit():
            return ComplexityResult("simple", 0.1)

        try:
            # Find best complexity match
            complexity, confidence = self._find_best_match(
                prompt, self.complexity_examples
            )

            # Find operation type
            operation_type, operation_confidence = self._find_best_match(
                prompt, self.operation_examples
            )

            return ComplexityResult(
                complexity=complexity,
                confidence=confidence,
                operation_type=operation_type,
                operation_confidence=operation_confidence,
            )

        except Exception as e:
            logger.error(f"Error in complexity detection: {e}")
            # Fallback to simple classification
            return ComplexityResult("simple", 0.5)

    def get_similar_prompts(
        self, prompt: str, top_k: int = 5
    ) -> list[tuple[str, str, float]]:
        """
        Find similar prompts from our example database.

        Args:
            prompt: The input prompt
            top_k: Number of similar prompts to return

        Returns:
            List of (prompt, category, similarity) tuples
        """
        if self.model is None:
            return []

        try:
            # Combine all examples
            all_examples = []
            for category, examples in self.complexity_examples.items():
                all_examples.extend([(ex, category) for ex in examples])

            if not all_examples:
                return []

            # Compute embeddings
            texts = [prompt] + [ex[0] for ex in all_examples]
            embeddings = self._compute_embeddings(texts)

            # Calculate similarities
            similarities = util.pytorch_cos_sim(embeddings[0:1], embeddings[1:])[0]

            # Get top-k results
            top_indices = similarities.argsort(descending=True)[:top_k]

            results = []
            for idx in top_indices:
                similarity = similarities[idx].item()
                example, category = all_examples[idx.item()]
                results.append((example, category, similarity))

            return results

        except Exception as e:
            logger.error(f"Error finding similar prompts: {e}")
            return []

    def is_multilingual_supported(self) -> bool:
        """Check if multilingual support is available."""
        return self.model is not None and SENTENCE_TRANSFORMERS_AVAILABLE


# Fallback simple detector for when sentence-transformers is not available
class SimpleComplexityDetector:
    """Simple fallback complexity detector based on word counting."""

    def __init__(self):
        self.operation_keywords = {
            "track": ["track", "create", "delete", "rename", "duplicate"],
            "volume": ["volume", "vol", "gain", "level", "db", "decibel"],
            "effect": [
                "reverb",
                "delay",
                "compression",
                "compressor",
                "chorus",
                "flanger",
            ],
            "clip": ["clip", "region", "bar", "measure"],
            "midi": ["midi", "note", "chord", "quantize", "transpose"],
        }

    def detect_complexity(self, prompt: str) -> ComplexityResult:
        """Simple complexity detection based on word count and keywords."""
        if not prompt or not prompt.strip():
            return ComplexityResult("simple", 0.1)

        words = prompt.lower().split()
        word_count = len(words)

        # Count operation keywords
        operation_counts = {}
        for op_type, keywords in self.operation_keywords.items():
            count = sum(
                1 for word in words if any(keyword in word for keyword in keywords)
            )
            operation_counts[op_type] = count

        total_operations = sum(operation_counts.values())

        # Simple classification logic
        if word_count <= 5 and total_operations <= 1:
            complexity = "simple"
            confidence = 0.8
        elif word_count <= 10 and total_operations <= 2:
            complexity = "medium"
            confidence = 0.7
        else:
            complexity = "complex"
            confidence = 0.6

        # Find most likely operation type
        if operation_counts:
            operation_type = max(operation_counts, key=operation_counts.get)
            operation_confidence = min(
                operation_counts[operation_type] / max(1, total_operations), 1.0
            )
        else:
            operation_type = None
            operation_confidence = None

        return ComplexityResult(
            complexity=complexity,
            confidence=confidence,
            operation_type=operation_type,
            operation_confidence=operation_confidence,
        )


def get_complexity_detector() -> Any:
    """
    Factory function to get the best available complexity detector.

    Returns:
        SemanticComplexityDetector if sentence-transformers is available and model loads successfully,
        otherwise SimpleComplexityDetector
    """
    if SENTENCE_TRANSFORMERS_AVAILABLE:
        try:
            detector = SemanticComplexityDetector()
            # Check if the model actually loaded
            if detector.model is not None:
                return detector
            else:
                logger.warning("Semantic model failed to load, using fallback detector")
                return SimpleComplexityDetector()
        except Exception as e:
            logger.warning(
                f"Failed to initialize semantic detector: {e}, using fallback"
            )
            return SimpleComplexityDetector()
    else:
        return SimpleComplexityDetector()


def ensure_model_downloaded(model_name: str = "all-MiniLM-L6-v2") -> bool:
    """
    Ensure the sentence transformer model is downloaded locally.

    Args:
        model_name: Name of the model to download

    Returns:
        True if model is available (either already cached or successfully downloaded)
    """
    if not SENTENCE_TRANSFORMERS_AVAILABLE:
        logger.warning("sentence-transformers not available")
        return False

    try:
        import os
        from pathlib import Path

        # Check if model exists locally
        cache_dir = Path.home() / ".cache" / "huggingface" / "hub"
        model_dir = cache_dir / f"models--sentence-transformers--{model_name}"

        if model_dir.exists():
            logger.info(f"Model already available locally: {model_dir}")
            return True

        # Model not found, download it
        logger.info(f"Model not found locally, downloading: {model_name}")

        # Temporarily remove offline flags to allow download
        old_offline = os.environ.get("HF_HUB_OFFLINE")
        os.environ.pop("HF_HUB_OFFLINE", None)
        os.environ.pop("TRANSFORMERS_OFFLINE", None)
        os.environ.pop("HF_DATASETS_OFFLINE", None)

        try:
            from sentence_transformers import SentenceTransformer

            # This will download the model
            model = SentenceTransformer(model_name)
            logger.info(f"Model downloaded successfully: {model_name}")
            return True
        finally:
            # Restore offline flags
            if old_offline:
                os.environ["HF_HUB_OFFLINE"] = old_offline
            else:
                os.environ.pop("HF_HUB_OFFLINE", None)

    except Exception as e:
        logger.error(f"Failed to download model {model_name}: {e}")
        return False
