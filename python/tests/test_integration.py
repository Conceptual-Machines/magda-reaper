"""
Integration tests for MAGDA that make real API calls.
Tests the full pipeline with various prompts including multilingual support.
"""

import os
from typing import Any

import pytest

from magda.pipeline import MAGDAPipeline


class TestIntegration:
    """Integration tests with real API calls."""

    @pytest.fixture
    def pipeline(self):
        """Create a pipeline instance for testing."""
        api_key = os.getenv("OPENAI_API_KEY")
        if not api_key:
            pytest.skip("OPENAI_API_KEY not set")
        return MAGDAPipeline()

    @pytest.fixture
    def test_prompts(self) -> list[dict[str, Any]]:
        """All the original test prompts we used."""
        return [
            # Basic track operations
            {
                "prompt": "Create a new track called 'Guitar'",
                "expected_type": "track",
                "description": "Basic track creation"
            },
            {
                "prompt": "Add a new audio track named 'Bass'",
                "expected_type": "track",
                "description": "Audio track creation"
            },
            {
                "prompt": "Delete the track 'Drums'",
                "expected_type": "track",
                "description": "Track deletion"
            },

            # Volume operations
            {
                "prompt": "Set the volume of track 'Guitar' to -6dB",
                "expected_type": "volume",
                "description": "Volume setting"
            },
            {
                "prompt": "Increase the volume of 'Bass' by 3dB",
                "expected_type": "volume",
                "description": "Volume adjustment"
            },
            {
                "prompt": "Mute the 'Drums' track",
                "expected_type": ["track", "volume"],  # Could be either
                "description": "Mute operation"
            },

            # Effect operations
            {
                "prompt": "Add reverb to the 'Guitar' track",
                "expected_type": "effect",
                "description": "Effect addition"
            },
            {
                "prompt": "Remove the delay from 'Bass'",
                "expected_type": "effect",
                "description": "Effect removal"
            },
            {
                "prompt": "Set the reverb wet level to 50% on 'Guitar'",
                "expected_type": ["effect", "volume"],  # Could be effect parameter or volume
                "description": "Effect parameter adjustment"
            },

            # Clip operations
            {
                "prompt": "Create a 4-bar clip on track 'Guitar'",
                "expected_type": "clip",
                "description": "Clip creation"
            },
            {
                "prompt": "Delete the clip at bar 8 on 'Bass'",
                "expected_type": "clip",
                "description": "Clip deletion"
            },
            {
                "prompt": "Move the clip from bar 4 to bar 12 on 'Drums'",
                "expected_type": "clip",
                "description": "Clip movement"
            },

            # MIDI operations
            {
                "prompt": "Add a C major chord at bar 1 on 'Piano'",
                "expected_type": "midi",
                "description": "MIDI note addition"
            },
            {
                "prompt": "Delete the note at beat 2.5 on 'Bass'",
                "expected_type": "midi",
                "description": "MIDI note deletion"
            },
            {
                "prompt": "Change the velocity of the note at bar 4 to 80",
                "expected_type": "midi",
                "description": "MIDI parameter adjustment"
            },

            # Multilingual prompts
            {
                "prompt": "Crea una nueva pista llamada 'Guitarra'",
                "expected_type": "track",
                "description": "Spanish track creation"
            },
            {
                "prompt": "Ajusta el volumen de la pista 'Bajo' a -3dB",
                "expected_type": "volume",
                "description": "Spanish volume adjustment"
            },
            {
                "prompt": "Ajoute une nouvelle piste audio nommée 'Batterie'",
                "expected_type": "track",
                "description": "French track creation"
            },
            {
                "prompt": "Füge einen Reverb-Effekt zur 'Gitarre'-Spur hinzu",
                "expected_type": "effect",
                "description": "German effect addition"
            },
            {
                "prompt": "トラック「ベース」の音量を-6dBに設定してください",
                "expected_type": "volume",
                "description": "Japanese volume setting"
            },

            # Complex multi-operation prompts
            {
                "prompt": "Create a new track called 'Lead', set its volume to -3dB, and add reverb",
                "expected_type": "track",  # Primary operation
                "description": "Multi-operation prompt"
            },
            {
                "prompt": "Add a new MIDI track 'Piano', create a 4-bar clip, and add a C major chord",
                "expected_type": "track",  # Primary operation
                "description": "Complex MIDI setup"
            }
        ]

    def test_operation_identification(self, pipeline, test_prompts):
        """Test that the operation identifier correctly identifies operations."""
        for test_case in test_prompts:
            prompt = test_case["prompt"]
            expected_type = test_case["expected_type"]
            description = test_case["description"]

            print(f"\nTesting: {description}")
            print(f"Prompt: {prompt}")

            # Test operation identification through full pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Pipeline result: {result}")

            assert result is not None, f"Pipeline failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check if the expected operation type is in the identified operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            if isinstance(expected_type, list):
                # For multiple possible types, check if any match
                assert any(exp_type in operation_types for exp_type in expected_type), \
                    f"Expected one of {expected_type} in {operation_types} for: {prompt}"
            else:
                # For single type
                assert expected_type in operation_types, \
                    f"Expected {expected_type} in {operation_types} for: {prompt}"

            print("✓ Operation identification passed")

    def test_track_agent_integration(self, pipeline):
        """Test track agent with real API calls."""
        track_prompts = [
            "Create a new track called 'Electric Guitar'",
            "Add a new audio track named 'Acoustic Bass'",
            "Delete the track 'Old Drums'",
            "Crea una nueva pista llamada 'Piano'",
            "Ajoute une piste MIDI nommée 'Synth'"
        ]

        for prompt in track_prompts:
            print(f"\nTesting track agent with: {prompt}")

            # Process through pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"Track agent failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for track operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert "track" in operation_types, f"No track operation found for: {prompt}"

            print("✓ Track agent passed")

    def test_volume_agent_integration(self, pipeline):
        """Test volume agent with real API calls."""
        volume_prompts = [
            "Set the volume of track 'Guitar' to -6dB",
            "Increase the volume of 'Bass' by 3dB",
            "Mute the 'Drums' track",
            "Ajusta el volumen de 'Piano' a -3dB",
            "Réduis le volume de 'Synth' de 2dB"
        ]

        for prompt in volume_prompts:
            print(f"\nTesting volume agent with: {prompt}")

            # Process through pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"Volume agent failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for volume operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert "volume" in operation_types, f"No volume operation found for: {prompt}"

            print("✓ Volume agent passed")

    def test_effect_agent_integration(self, pipeline):
        """Test effect agent with real API calls."""
        effect_prompts = [
            "Add reverb to the 'Guitar' track",
            "Remove the delay from 'Bass'",
            "Set the reverb wet level to 50% on 'Guitar'",
            "Füge einen Chorus-Effekt zur 'Piano'-Spur hinzu",
            "Ajoute un filtre passe-bas à la piste 'Synth'"
        ]

        for prompt in effect_prompts:
            print(f"\nTesting effect agent with: {prompt}")

            # Process through pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"Effect agent failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for effect operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert "effect" in operation_types, f"No effect operation found for: {prompt}"

            print("✓ Effect agent passed")

    def test_clip_agent_integration(self, pipeline):
        """Test clip agent with real API calls."""
        clip_prompts = [
            "Create a 4-bar clip on track 'Guitar'",
            "Delete the clip at bar 8 on 'Bass'",
            "Move the clip from bar 4 to bar 12 on 'Drums'",
            "Crea un clip de 8 compases en la pista 'Piano'",
            "Crée un clip de 2 mesures sur la piste 'Synth'"
        ]

        for prompt in clip_prompts:
            print(f"\nTesting clip agent with: {prompt}")

            # Process through pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"Clip agent failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for clip operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert "clip" in operation_types, f"No clip operation found for: {prompt}"

            print("✓ Clip agent passed")

    def test_midi_agent_integration(self, pipeline):
        """Test MIDI agent with real API calls."""
        midi_prompts = [
            "Add a C major chord at bar 1 on 'Piano'",
            "Delete the note at beat 2.5 on 'Bass'",
            "Change the velocity of the note at bar 4 to 80",
            "Añade una nota Do en el compás 2 en 'Piano'",
            "Ajoute un accord de La mineur à la mesure 4 sur 'Synth'"
        ]

        for prompt in midi_prompts:
            print(f"\nTesting MIDI agent with: {prompt}")

            # Process through pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"MIDI agent failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for MIDI operations
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert "midi" in operation_types, f"No MIDI operation found for: {prompt}"

            print("✓ MIDI agent passed")

    def test_full_pipeline_integration(self, pipeline):
        """Test the complete pipeline end-to-end."""
        complex_prompts = [
            "Create a new track called 'Lead Guitar', set its volume to -3dB, and add reverb with 40% wet level",
            "Add a new MIDI track 'Piano', create a 4-bar clip, and add a C major chord at bar 1",
            "Crea una pista llamada 'Bajo', ajusta el volumen a -6dB, y añade un efecto de compresión",
            "Ajoute une piste audio 'Batterie', crée un clip de 8 mesures, et ajoute un filtre passe-haut"
        ]

        for prompt in complex_prompts:
            print(f"\nTesting full pipeline with: {prompt}")

            # Process through full pipeline
            result = pipeline.process_prompt(prompt)
            print(f"Pipeline result: {result}")

            assert result is not None, f"Pipeline failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert "daw_commands" in result, f"No DAW commands generated for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            print("✓ Full pipeline passed")

    def test_error_handling(self, pipeline):
        """Test error handling with invalid prompts."""
        invalid_prompts = [
            "This is not a valid DAW command",
            "Random text that doesn't make sense",
            "123456789",
            ""
        ]

        for prompt in invalid_prompts:
            print(f"\nTesting error handling with: {prompt}")

            try:
                result = pipeline.process_prompt(prompt)
                # Should either return None or have no operations
                if result is not None:
                    if "operations" in result and len(result["operations"]) == 0:
                        print("✓ No operations identified (expected)")
                    else:
                        print(f"Unexpected result: {result}")
            except Exception as e:
                print(f"Expected error: {e}")
                print("✓ Error handling passed")
            else:
                print(f"✓ No error (result: {result})")

    def test_multilingual_support(self, pipeline):
        """Test multilingual support with various languages."""
        multilingual_prompts = [
            # Spanish
            ("Crea una nueva pista llamada 'Guitarra Española'", "track"),
            ("Ajusta el volumen de 'Piano' a -3dB", "volume"),
            ("Añade reverb a la pista 'Bajo'", "effect"),

            # French
            ("Ajoute une nouvelle piste audio nommée 'Batterie Française'", "track"),
            ("Réduis le volume de 'Synth' de 2dB", "volume"),
            ("Ajoute un filtre passe-bas à la piste 'Guitare'", "effect"),

            # German
            ("Füge eine neue MIDI-Spur namens 'Klavier' hinzu", "track"),
            ("Erhöhe die Lautstärke der 'Bass'-Spur um 3dB", "volume"),
            ("Füge einen Chorus-Effekt zur 'Gitarre'-Spur hinzu", "effect"),

            # Italian
            ("Crea una nuova traccia chiamata 'Chitarra Italiana'", "track"),
            ("Imposta il volume della traccia 'Piano' a -6dB", "volume"),
            ("Aggiungi riverbero alla traccia 'Basso'", "effect"),

            # Portuguese
            ("Crie uma nova faixa chamada 'Guitarra Portuguesa'", "track"),
            ("Ajuste o volume da faixa 'Piano' para -3dB", "volume"),
            ("Adicione reverb à faixa 'Baixo'", "effect"),
        ]

        for prompt, expected_type in multilingual_prompts:
            print(f"\nTesting multilingual: {prompt}")

            result = pipeline.process_prompt(prompt)
            print(f"Result: {result}")

            assert result is not None, f"Multilingual processing failed for: {prompt}"
            assert "operations" in result, f"No operations found for: {prompt}"
            assert len(result["operations"]) > 0, f"No operations identified for: {prompt}"

            # Check for expected operation type
            operation_types = [op.get("type", "") for op in result["operations"]]
            assert expected_type in operation_types, \
                f"Expected {expected_type} in {operation_types} for: {prompt}"

            print("✓ Multilingual support passed")


if __name__ == "__main__":
    # Run integration tests directly
    pytest.main([__file__, "-v", "-s"])
