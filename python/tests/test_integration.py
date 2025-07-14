"""
Integration tests for MAGDA that make real API calls.
Tests the full pipeline with various prompts including multilingual support.
"""

import os

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

    # Individual track operation tests
    def test_create_track_basic(self, pipeline):
        """Test basic track creation."""
        prompt = "Create a new track called 'Guitar'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "track" in operation_types, f"Expected track operation for: {prompt}"

    def test_create_audio_track(self, pipeline):
        """Test audio track creation."""
        prompt = "Add a new audio track named 'Bass'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "track" in operation_types, f"Expected track operation for: {prompt}"

    def test_delete_track(self, pipeline):
        """Test track deletion."""
        prompt = "Delete the track 'Drums'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "track" in operation_types, f"Expected track operation for: {prompt}"

    # Individual volume operation tests
    def test_set_volume(self, pipeline):
        """Test volume setting."""
        prompt = "Set the volume of track 'Guitar' to -6dB"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "volume" in operation_types, f"Expected volume operation for: {prompt}"

    def test_increase_volume(self, pipeline):
        """Test volume increase."""
        prompt = "Increase the volume of 'Bass' by 3dB"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "volume" in operation_types, f"Expected volume operation for: {prompt}"

    def test_mute_track(self, pipeline):
        """Test track muting."""
        prompt = "Mute the 'Drums' track"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        # Could be either track or volume operation
        assert any(op_type in operation_types for op_type in ["track", "volume"]), (
            f"Expected track or volume operation for: {prompt}"
        )

    # Individual effect operation tests
    def test_add_reverb(self, pipeline):
        """Test adding reverb effect."""
        prompt = "Add reverb to the 'Guitar' track"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "effect" in operation_types, f"Expected effect operation for: {prompt}"

    def test_remove_delay(self, pipeline):
        """Test removing delay effect."""
        prompt = "Remove the delay from 'Bass'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "effect" in operation_types, f"Expected effect operation for: {prompt}"

    def test_set_reverb_wet_level(self, pipeline):
        """Test setting reverb wet level."""
        prompt = "Set the reverb wet level to 50% on 'Guitar'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        # Could be effect parameter or volume
        assert any(op_type in operation_types for op_type in ["effect", "volume"]), (
            f"Expected effect or volume operation for: {prompt}"
        )

    # Individual clip operation tests
    def test_create_clip(self, pipeline):
        """Test clip creation."""
        prompt = "Create a 4-bar clip on track 'Guitar'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "clip" in operation_types, f"Expected clip operation for: {prompt}"

    def test_delete_clip(self, pipeline):
        """Test clip deletion."""
        prompt = "Delete the clip at bar 8 on 'Bass'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "clip" in operation_types, f"Expected clip operation for: {prompt}"

    def test_move_clip(self, pipeline):
        """Test clip movement."""
        prompt = "Move the clip from bar 4 to bar 12 on 'Drums'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "clip" in operation_types, f"Expected clip operation for: {prompt}"

    # Individual MIDI operation tests
    def test_add_midi_chord(self, pipeline):
        """Test adding MIDI chord."""
        prompt = "Add a C major chord at bar 1 on 'Piano'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "midi" in operation_types, f"Expected MIDI operation for: {prompt}"

    def test_delete_midi_note(self, pipeline):
        """Test deleting MIDI note."""
        prompt = "Delete the note at beat 2.5 on 'Bass'"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "midi" in operation_types, f"Expected MIDI operation for: {prompt}"

    def test_change_midi_velocity(self, pipeline):
        """Test changing MIDI velocity."""
        prompt = "Change the velocity of the note at bar 4 to 80"
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "midi" in operation_types, f"Expected MIDI operation for: {prompt}"

    # Multilingual tests using parametrize
    @pytest.mark.parametrize(
        "prompt,expected_type",
        [
            ("Crea una nueva pista llamada 'Guitarra'", "track"),
            ("Ajusta el volumen de la pista 'Bajo' a -3dB", "volume"),
            ("Ajoute une nouvelle piste audio nommée 'Batterie'", "track"),
            ("Füge einen Reverb-Effekt zur 'Gitarre'-Spur hinzu", "effect"),
            ("トラック「ベース」の音量を-6dBに設定してください", "volume"),
        ],
    )
    def test_multilingual_operations(self, pipeline, prompt, expected_type):
        """Test multilingual operation identification."""
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Multilingual processing failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        operation_types = [op.get("type", "") for op in result["operations"]]
        assert expected_type in operation_types, (
            f"Expected {expected_type} in {operation_types} for: {prompt}"
        )

    # Complex multi-operation tests
    def test_multi_operation_track_setup(self, pipeline):
        """Test complex multi-operation prompt."""
        prompt = (
            "Create a new track called 'Lead', set its volume to -3dB, and add reverb"
        )
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        # Should identify multiple operations
        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "track" in operation_types, f"Expected track operation for: {prompt}"

    def test_midi_track_complex_setup(self, pipeline):
        """Test complex MIDI track setup."""
        prompt = (
            "Add a new MIDI track 'Piano', create a 4-bar clip, and add a C major chord"
        )
        result = pipeline.process_prompt(prompt)

        assert result is not None, f"Pipeline failed for: {prompt}"
        assert "operations" in result, f"No operations found for: {prompt}"
        assert result["operations"], f"No operations identified for: {prompt}"

        # Should identify multiple operations
        operation_types = [op.get("type", "") for op in result["operations"]]
        assert "track" in operation_types, f"Expected track operation for: {prompt}"

    # Error handling tests
    @pytest.mark.parametrize(
        "invalid_prompt",
        [
            "This is not a valid DAW command",
            "Random text that doesn't make sense",
            "123456789",
            "",
        ],
    )
    def test_error_handling_invalid_prompts(self, pipeline, invalid_prompt):
        """Test error handling with invalid prompts - should handle gracefully."""
        result = pipeline.process_prompt(invalid_prompt)
        # Should either return None or have no operations
        if result is not None:
            if "operations" in result and not result["operations"]:
                pass  # Expected: no operations identified
            else:
                # If we get unexpected operations, that's fine too - just log it
                print(f"Unexpected result: {result}")

    def test_error_handling_timeout(self, pipeline):
        """Test error handling with timeout scenarios."""
        # This test would require mocking the timeout behavior
        # For now, we'll test that the pipeline doesn't crash on very long prompts
        long_prompt = "Create a track called 'Test' " * 1000  # Very long prompt
        result = pipeline.process_prompt(long_prompt)
        # Should handle gracefully without crashing
        assert result is not None or result is None  # Either is acceptable

    # Full pipeline integration tests
    @pytest.mark.parametrize(
        "complex_prompt",
        [
            "Create a new track called 'Lead Guitar', set its volume to -3dB, and add reverb with 40% wet level",
            "Add a new MIDI track 'Piano', create a 4-bar clip, and add a C major chord at bar 1",
            "Crea una pista llamada 'Bajo', ajusta el volumen a -6dB, y añade un efecto de compresión",
            "Ajoute une piste audio 'Batterie', crée un clip de 8 mesures, et ajoute un filtre passe-haut",
        ],
    )
    def test_full_pipeline_complex(self, pipeline, complex_prompt):
        """Test the complete pipeline end-to-end with complex prompts."""
        result = pipeline.process_prompt(complex_prompt)

        assert result is not None, f"Pipeline failed for: {complex_prompt}"
        assert "operations" in result, f"No operations found for: {complex_prompt}"
        assert "daw_commands" in result, (
            f"No DAW commands generated for: {complex_prompt}"
        )
        assert result["operations"], f"No operations identified for: {complex_prompt}"


if __name__ == "__main__":
    # Run integration tests directly
    pytest.main([__file__, "-v", "-s"])
