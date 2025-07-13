"""
Simple integration tests for quick verification.
"""

from unittest.mock import patch

import pytest

from magda.pipeline import MAGDAPipeline


@pytest.mark.integration
class TestSimpleIntegration:
    """Simple integration tests with mocked API calls."""

    @pytest.fixture
    def pipeline(self):
        """Create a pipeline instance."""
        return MAGDAPipeline()

    @patch(
        "magda.agents.operation_identifier.OperationIdentifier.identify_operations_with_llm"
    )
    def test_basic_track_creation(self, mock_identify, pipeline):
        """Test basic track creation."""
        prompt = "Create a new track called 'Test Guitar'"

        # Mock operation identification
        mock_identify.return_value = [
            {
                "type": "track",
                "description": "Create a new track called 'Test Guitar'",
                "parameters": {"name": "Test Guitar"},
            }
        ]

        # Mock track agent response
        with patch.object(pipeline.agents["track"], "execute") as mock_track:
            mock_track.return_value = {
                "daw_command": "track(Test Guitar)",
                "result": {"id": "track_1", "name": "Test Guitar"},
            }

            print(f"\nTesting: {prompt}")
            result = pipeline.process_prompt(prompt)

            assert result is not None
            assert "operations" in result
            assert "daw_commands" in result
            assert len(result["operations"]) > 0
            print(f"✓ Track creation passed: {result}")

    @patch(
        "magda.agents.operation_identifier.OperationIdentifier.identify_operations_with_llm"
    )
    def test_volume_adjustment(self, mock_identify, pipeline):
        """Test volume adjustment."""
        prompt = "Set the volume of track 'Test Guitar' to -3dB"

        # Mock operation identification
        mock_identify.return_value = [
            {
                "type": "volume",
                "description": "Set the volume of track 'Test Guitar' to -3dB",
                "parameters": {"track_name": "Test Guitar", "volume": -3},
            }
        ]

        # Mock volume agent response
        with patch.object(pipeline.agents["volume"], "execute") as mock_volume:
            mock_volume.return_value = {
                "daw_command": "volume(Test Guitar, -3)",
                "result": {"id": "volume_1", "track_id": "track_1", "volume": -3},
            }

            print(f"\nTesting: {prompt}")
            result = pipeline.process_prompt(prompt)

            assert result is not None
            assert "operations" in result
            assert "daw_commands" in result
            assert len(result["operations"]) > 0
            print(f"✓ Volume adjustment passed: {result}")

    @patch(
        "magda.agents.operation_identifier.OperationIdentifier.identify_operations_with_llm"
    )
    def test_effect_addition(self, mock_identify, pipeline):
        """Test effect addition."""
        prompt = "Add reverb to the 'Test Guitar' track"

        # Mock operation identification
        mock_identify.return_value = [
            {
                "type": "effect",
                "description": "Add reverb to the 'Test Guitar' track",
                "parameters": {"track_name": "Test Guitar", "effect": "reverb"},
            }
        ]

        # Mock effect agent response
        with patch.object(pipeline.agents["effect"], "execute") as mock_effect:
            mock_effect.return_value = {
                "daw_command": "effect(Test Guitar, reverb)",
                "result": {"id": "effect_1", "track_id": "track_1", "type": "reverb"},
            }

            print(f"\nTesting: {prompt}")
            result = pipeline.process_prompt(prompt)

            assert result is not None
            assert "operations" in result
            assert "daw_commands" in result
            assert len(result["operations"]) > 0
            print(f"✓ Effect addition passed: {result}")

    @patch(
        "magda.agents.operation_identifier.OperationIdentifier.identify_operations_with_llm"
    )
    def test_spanish_prompt(self, mock_identify, pipeline):
        """Test Spanish prompt."""
        prompt = "Crea una nueva pista llamada 'Piano Español'"

        # Mock operation identification
        mock_identify.return_value = [
            {
                "type": "track",
                "description": "Create a new track called 'Piano Español'",
                "parameters": {"name": "Piano Español"},
            }
        ]

        # Mock track agent response
        with patch.object(pipeline.agents["track"], "execute") as mock_track:
            mock_track.return_value = {
                "daw_command": "track(Piano Español)",
                "result": {"id": "track_1", "name": "Piano Español"},
            }

            print(f"\nTesting: {prompt}")
            result = pipeline.process_prompt(prompt)

            assert result is not None
            assert "operations" in result
            assert "daw_commands" in result
            assert len(result["operations"]) > 0
            print(f"✓ Spanish prompt passed: {result}")

    @patch(
        "magda.agents.operation_identifier.OperationIdentifier.identify_operations_with_llm"
    )
    def test_french_prompt(self, mock_identify, pipeline):
        """Test French prompt."""
        prompt = "Ajoute une piste audio nommée 'Basse Française'"

        # Mock operation identification
        mock_identify.return_value = [
            {
                "type": "track",
                "description": "Create a new track called 'Basse Française'",
                "parameters": {"name": "Basse Française"},
            }
        ]

        # Mock track agent response
        with patch.object(pipeline.agents["track"], "execute") as mock_track:
            mock_track.return_value = {
                "daw_command": "track(Basse Française)",
                "result": {"id": "track_1", "name": "Basse Française"},
            }

            print(f"\nTesting: {prompt}")
            result = pipeline.process_prompt(prompt)

            assert result is not None
            assert "operations" in result
            assert "daw_commands" in result
            assert len(result["operations"]) > 0
            print(f"✓ French prompt passed: {result}")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
