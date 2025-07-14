#!/usr/bin/env python3
"""
Test script for sentence-transformers in MAGDA complexity detection and beyond.
"""

import os

# Set offline mode BEFORE importing sentence-transformers
os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["TRANSFORMERS_OFFLINE"] = "1"
os.environ["HF_DATASETS_OFFLINE"] = "1"
os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"

from sentence_transformers import SentenceTransformer, util


class MAGDASentenceTransformer:
    """Enhanced complexity detection and operation classification using sentence-transformers."""

    def __init__(self, model_name: str = "all-MiniLM-L6-v2"):
        """Initialize with a local sentence transformer model."""
        print(f"Loading model: {model_name}")
        self.model = SentenceTransformer(model_name)
        print("Model loaded successfully!")

        # Example prompts for complexity classification
        self.complexity_examples = {
            "simple": [
                "Set the volume to -6dB",
                "Create a new track called 'Guitar'",
                "Mute the drums",
                "Add reverb to track 'Bass'",
                "Delete the clip at bar 8",
                "Set volume to 75%",
                "Create a MIDI track",
                "Add delay effect",
                "Mute track 'Piano'",
                "Set pan to center",
            ],
            "medium": [
                "Create a new track and add reverb",
                "Mute the drums and set the bass volume to -3dB",
                "Add compression to the guitar track",
                "Create a 4-bar clip and add a C major chord",
                "Set the volume and pan for the lead track",
                "Add reverb with 40% wet level",
                "Create a track and set its volume to -6dB",
                "Add delay with 0.5s timing",
                "Mute all tracks except drums",
                "Set the compressor threshold to -20dB",
            ],
            "complex": [
                "Create a new track, add reverb, set the volume to -3dB, and export the mix",
                "For all tracks except drums, add a compressor and set the threshold to -10dB",
                "Create a bass track with Serum, add compression with 4:1 ratio, set volume to -6dB, and pan left",
                "If the guitar track exists, add reverb with 30% wet, otherwise create it first",
                "Create a new project with 8 tracks, add effects to each, and set up a master bus",
                "For tracks 1-4, add compression and EQ, for tracks 5-8, add reverb and delay",
                "Create a drum bus, route all drum tracks to it, add compression, and set the overall level",
                "Set up a sidechain compression where the kick triggers compression on the bass track",
                "Create a vocal chain with EQ, compression, reverb, and delay in series",
                "Export the mix as both WAV and MP3, normalize to -14 LUFS, and add metadata",
            ],
        }

        # Example prompts for operation classification
        self.operation_examples = {
            "track": [
                "Create a new track called 'Guitar'",
                "Add a new audio track",
                "Create a MIDI track named 'Piano'",
                "Add a new track for drums",
                "Create a track with Serum",
                "Add a new track called 'Bass'",
                "Create an audio track",
                "Add a new MIDI track",
                "Create a track for vocals",
                "Add a new track with Kontakt",
            ],
            "volume": [
                "Set the volume to -6dB",
                "Adjust the volume of track 'Bass'",
                "Set volume to 75%",
                "Change the volume to -3dB",
                "Set the track volume",
                "Adjust volume levels",
                "Set volume to maximum",
                "Change volume to 50%",
                "Set the master volume",
                "Adjust the volume slider",
            ],
            "effect": [
                "Add reverb to the track",
                "Add compression to 'Guitar'",
                "Add delay effect",
                "Add EQ to the bass",
                "Add chorus effect",
                "Add distortion to guitar",
                "Add reverb with 40% wet",
                "Add compressor with 4:1 ratio",
                "Add delay with 0.5s timing",
                "Add filter effect",
            ],
            "clip": [
                "Create a 4-bar clip",
                "Delete the clip at bar 8",
                "Move the clip to bar 12",
                "Create a clip on track 'Guitar'",
                "Delete the audio clip",
                "Move the clip forward",
                "Create a 8-bar clip",
                "Delete the MIDI clip",
                "Move the clip backward",
                "Create a clip from bar 1 to 4",
            ],
            "midi": [
                "Add a C major chord",
                "Add a note at bar 1",
                "Delete the MIDI note",
                "Add a chord progression",
                "Quantize the MIDI notes",
                "Add a bass line",
                "Delete the chord at bar 4",
                "Add a melody line",
                "Transpose the notes up 2 semitones",
                "Add a drum pattern",
            ],
        }

        # Multilingual examples for similarity testing
        self.multilingual_examples = {
            "track_creation": [
                "Create a new track called 'Guitar'",
                "Crea una nueva pista llamada 'Guitarra'",
                "Ajoute une nouvelle piste nommée 'Guitare'",
                "Füge eine neue Spur namens 'Gitarre' hinzu",
                "Crea una nuova traccia chiamata 'Chitarra'",
                "Crie uma nova faixa chamada 'Violão'",
                "新しいトラック「ギター」を作成してください",
            ],
            "volume_adjustment": [
                "Set the volume to -6dB",
                "Ajusta el volumen a -6dB",
                "Régler le volume à -6dB",
                "Lautstärke auf -6dB einstellen",
                "Imposta il volume a -6dB",
                "Ajuste o volume para -6dB",
                "音量を-6dBに設定してください",
            ],
        }

        # Pre-compute embeddings
        self._compute_embeddings()

    def _compute_embeddings(self):
        """Pre-compute embeddings for all examples."""
        print("Computing embeddings...")

        # Complexity embeddings
        self.complexity_prompts = []
        self.complexity_labels = []
        for label, prompts in self.complexity_examples.items():
            self.complexity_prompts.extend(prompts)
            self.complexity_labels.extend([label] * len(prompts))
        self.complexity_embeddings = self.model.encode(self.complexity_prompts)

        # Operation embeddings
        self.operation_prompts = []
        self.operation_labels = []
        for label, prompts in self.operation_examples.items():
            self.operation_prompts.extend(prompts)
            self.operation_labels.extend([label] * len(prompts))
        self.operation_embeddings = self.model.encode(self.operation_prompts)

        # Multilingual embeddings
        self.multilingual_prompts = []
        self.multilingual_categories = []
        for category, prompts in self.multilingual_examples.items():
            self.multilingual_prompts.extend(prompts)
            self.multilingual_categories.extend([category] * len(prompts))
        self.multilingual_embeddings = self.model.encode(self.multilingual_prompts)

        print("Embeddings computed!")

    def classify_complexity(self, prompt: str) -> tuple[str, float]:
        """Classify prompt complexity using similarity to examples."""
        prompt_emb = self.model.encode(prompt)
        similarities = util.cos_sim(prompt_emb, self.complexity_embeddings)[0]
        best_idx = similarities.argmax().item()
        return self.complexity_labels[best_idx], similarities[best_idx].item()

    def classify_operation(self, prompt: str) -> tuple[str, float]:
        """Classify operation type using similarity to examples."""
        prompt_emb = self.model.encode(prompt)
        similarities = util.cos_sim(prompt_emb, self.operation_embeddings)[0]
        best_idx = similarities.argmax().item()
        return self.operation_labels[best_idx], similarities[best_idx].item()

    def find_multilingual_similarity(
        self, prompt: str, category: str = None
    ) -> list[tuple[str, float]]:
        """Find similar prompts across languages."""
        prompt_emb = self.model.encode(prompt)
        similarities = util.cos_sim(prompt_emb, self.multilingual_embeddings)[0]

        # Get top 5 similar prompts
        top_indices = similarities.argsort(descending=True)[:5]
        results = []
        for idx in top_indices:
            similar_prompt = self.multilingual_prompts[idx.item()]
            category_name = self.multilingual_categories[idx.item()]
            similarity = similarities[idx].item()

            if category is None or category_name == category:
                results.append((similar_prompt, similarity, category_name))

        return results[:5]


def test_complexity_detection():
    """Test the sentence-transformer based complexity detection."""
    print("=== Testing Sentence-Transformer Based Complexity Detection ===\n")

    # Initialize the model
    magda_st = MAGDASentenceTransformer()

    # Test prompts
    test_prompts = [
        # Simple prompts
        "Set the volume to -6dB",
        "Create a new track called 'Guitar'",
        "Mute the drums",
        "Add reverb to track 'Bass'",
        # Medium complexity
        "Create a new track and add reverb",
        "Mute the drums and set the bass volume to -3dB",
        "Add compression to the guitar track",
        "Create a 4-bar clip and add a C major chord",
        # Complex prompts
        "Create a new track, add reverb, set the volume to -3dB, and export the mix",
        "For all tracks except drums, add a compressor and set the threshold to -10dB",
        "Create a bass track with Serum, add compression with 4:1 ratio, set volume to -6dB, and pan left",
        "If the guitar track exists, add reverb with 30% wet, otherwise create it first",
        # Edge cases
        "This is not a valid DAW command",
        "Random text that doesn't make sense",
        "123456789",
        "",
    ]

    print("Complexity Classification Results:")
    print("-" * 80)
    for prompt in test_prompts:
        complexity, confidence = magda_st.classify_complexity(prompt)
        print(f"Prompt: {prompt}")
        print(f"  → Complexity: {complexity} (confidence: {confidence:.3f})")
        print()

    print("\n=== Testing Operation Classification ===")
    print("-" * 80)

    operation_test_prompts = [
        "Create a new track called 'Lead Guitar'",
        "Set the volume of track 'Bass' to -6dB",
        "Add reverb to the guitar track",
        "Create a 4-bar clip on track 'Drums'",
        "Add a C major chord at bar 1",
        "Delete the track 'Old Drums'",
        "Adjust the volume to 75%",
        "Add compression with 4:1 ratio",
        "Move the clip to bar 12",
        "Quantize the MIDI notes to 16th notes",
    ]

    for prompt in operation_test_prompts:
        operation, confidence = magda_st.classify_operation(prompt)
        print(f"Prompt: {prompt}")
        print(f"  → Operation: {operation} (confidence: {confidence:.3f})")
        print()

    print("\n=== Testing Multilingual Similarity ===")
    print("-" * 80)

    # Test multilingual similarity
    test_multilingual_prompts = [
        "Create a new track called 'Guitar'",
        "Set the volume to -6dB",
        "Crea una nueva pista llamada 'Guitarra'",
        "Ajusta el volumen a -6dB",
        "Ajoute une nouvelle piste nommée 'Guitare'",
        "Régler le volume à -6dB",
        "新しいトラック「ギター」を作成してください",
        "音量を-6dBに設定してください",
    ]

    for prompt in test_multilingual_prompts:
        print(f"Prompt: {prompt}")
        similar_prompts = magda_st.find_multilingual_similarity(prompt)
        print("  Similar prompts:")
        for similar_prompt, similarity, category in similar_prompts:
            print(f"    - {similar_prompt} ({category}, similarity: {similarity:.3f})")
        print()


if __name__ == "__main__":
    test_complexity_detection()
