import pytest
from unittest.mock import Mock, patch
from magda.pipeline import MAGDAPipeline
from magda.models import Operation, OperationType, AgentResponse


class TestMAGDAPipeline:
    """Test cases for the MAGDA pipeline."""

    @pytest.fixture
    def pipeline(self):
        """Create a MAGDA pipeline instance for testing."""
        return MAGDAPipeline()

    @pytest.fixture
    def mock_operation_identifier(self):
        """Mock operation identifier response."""
        return [
            {
                "type": "track",
                "description": "Create a bass track with Serum",
                "parameters": {"track_name": "bass", "instrument": "serum"}
            },
            {
                "type": "track",
                "description": "Create a drums track with Addictive Drums",
                "parameters": {"track_name": "drums", "instrument": "addictive_drums"}
            }
        ]

    @pytest.fixture
    def mock_track_agent_response(self):
        """Mock track agent response."""
        return AgentResponse(
            daw_commands=["track(bass, serum)", "track(drums, addictive_drums)"],
            context={"tracks": {"bass": "track_1", "drums": "track_2"}},
            reasoning="Created bass and drums tracks with specified instruments"
        )

    def test_pipeline_initialization(self, pipeline):
        """Test that pipeline initializes correctly."""
        assert pipeline is not None
        assert hasattr(pipeline, 'operation_identifier')
        assert hasattr(pipeline, 'agents')

    @patch('magda.agents.operation_identifier.OperationIdentifier.execute')
    def test_process_prompt_single_operation(self, mock_identifier, pipeline, mock_operation_identifier):
        """Test processing a prompt with a single operation."""
        mock_identifier.return_value = {"operations": mock_operation_identifier[:1]}
        
        with patch.object(pipeline.agents['track'], 'execute') as mock_track:
            mock_track.return_value = {
                "daw_command": "track(bass, serum)",
                "result": {"id": "track_1", "name": "bass", "instrument": "serum"},
                "context": {"tracks": {"bass": "track_1"}}
            }
            
            result = pipeline.process_prompt("create a bass track with serum")
            
            assert result["daw_commands"] == ["track(bass, serum)"]

    @patch('magda.agents.operation_identifier.OperationIdentifier.execute')
    def test_process_prompt_multiple_operations(self, mock_identifier, pipeline, mock_operation_identifier):
        """Test processing a prompt with multiple operations."""
        mock_identifier.return_value = {"operations": mock_operation_identifier}
        
        with patch.object(pipeline.agents['track'], 'execute') as mock_track:
            mock_track.return_value = {
                "daw_command": "track(bass, serum); track(drums, addictive_drums)",
                "result": {"id": "track_1", "name": "bass", "instrument": "serum"},
                "context": {"tracks": {"bass": "track_1", "drums": "track_2"}}
            }
            
            result = pipeline.process_prompt("create bass and drums tracks")
            
            assert len(result["daw_commands"]) == 2
            assert "track(bass, serum)" in result["daw_commands"][0]
            assert "track(drums, addictive_drums)" in result["daw_commands"][1]

    @patch('magda.agents.operation_identifier.OperationIdentifier.execute')
    def test_process_prompt_context_passing(self, mock_identifier, pipeline):
        """Test that context is passed between operations."""
        operations = [
            {
                "type": "track",
                "description": "Create a bass track",
                "parameters": {"track_name": "bass"}
            },
            {
                "type": "effect",
                "description": "Add compressor to bass track",
                "parameters": {"track_name": "bass", "effect": "compressor"}
            }
        ]
        mock_identifier.return_value = {"operations": operations}
        
        with patch.object(pipeline.agents['track'], 'execute') as mock_track:
            mock_track.return_value = {
                "daw_command": "track(bass)",
                "result": {"id": "track_1", "name": "bass"},
                "context": {"tracks": {"bass": "track_1"}}
            }
            
            with patch.object(pipeline.agents['effect'], 'execute') as mock_effect:
                mock_effect.return_value = {
                    "daw_command": "effect(bass, compressor)",
                    "result": {"id": "effect_1", "type": "compressor"},
                    "context": {"tracks": {"bass": "track_1"}}
                }
                
                result = pipeline.process_prompt("create bass track and add compressor")
                
                # Verify effect agent was called with context from track agent
                mock_effect.assert_called_once()
                call_args = mock_effect.call_args[0][1]  # context is second argument
                assert call_args["track_id"] == "track_1"

    def test_process_prompt_empty_operations(self, pipeline):
        """Test handling of empty operations list."""
        with patch('magda.agents.operation_identifier.OperationIdentifier.execute') as mock_identifier:
            mock_identifier.return_value = {"operations": []}
            
            result = pipeline.process_prompt("this should return no operations")
            
            assert result["daw_commands"] == []
            assert result["results"] == []

    @patch('magda.agents.operation_identifier.OperationIdentifier.execute')
    def test_process_prompt_agent_not_found(self, mock_identifier, pipeline):
        """Test handling of unknown agent types."""
        operations = [
            {
                "type": "unknown_operation",
                "description": "Unknown operation",
                "parameters": {"track_name": "bass"}
            }
        ]
        mock_identifier.return_value = {"operations": operations}
        
        # Should not raise KeyError, just skip unknown operations
        result = pipeline.process_prompt("create bass track")
        assert result["daw_commands"] == []

    def test_pipeline_error_handling(self, pipeline):
        """Test pipeline error handling."""
        with patch('magda.agents.operation_identifier.OperationIdentifier.execute') as mock_identifier:
            mock_identifier.side_effect = Exception("API Error")
            
            with pytest.raises(Exception):
                pipeline.process_prompt("test prompt")


class TestPipelineIntegration:
    """Integration tests for the pipeline."""

    @pytest.fixture
    def pipeline(self):
        """Create a pipeline instance for integration testing."""
        return MAGDAPipeline()

    @pytest.mark.integration
    def test_simple_track_creation(self, pipeline):
        """Integration test for simple track creation."""
        # This would require actual API calls, so we'll mock them
        with patch('magda.agents.operation_identifier.OperationIdentifier.execute') as mock_identifier:
            mock_identifier.return_value = {
                "operations": [
                    {
                        "type": "track",
                        "description": "Create a bass track with Serum",
                        "parameters": {"track_name": "bass", "instrument": "serum"}
                    }
                ]
            }
            
            with patch.object(pipeline.agents['track'], 'execute') as mock_track:
                mock_track.return_value = {
                    "daw_command": "track(bass, serum)",
                    "result": {"id": "track_1", "name": "bass", "instrument": "serum"},
                    "context": {"tracks": {"bass": "track_1"}}
                }
                
                result = pipeline.process_prompt("create a bass track with serum")
                
                assert result["daw_commands"] == ["track(bass, serum)"]

    @pytest.mark.integration
    def test_complex_multi_step_operation(self, pipeline):
        """Integration test for complex multi-step operations."""
        operations = [
            {
                "type": "track",
                "description": "Create a bass track with Serum",
                "parameters": {"track_name": "bass", "instrument": "serum"}
            },
            {
                "type": "effect",
                "description": "Add compressor with 4:1 ratio to bass track",
                "parameters": {"track_name": "bass", "effect": "compressor", "ratio": 4}
            },
            {
                "type": "volume",
                "description": "Set bass track volume to -6dB",
                "parameters": {"track_name": "bass", "volume": -6}
            }
        ]
        
        with patch('magda.agents.operation_identifier.OperationIdentifier.execute') as mock_identifier:
            mock_identifier.return_value = {"operations": operations}
            
            # Mock track agent
            with patch.object(pipeline.agents['track'], 'execute') as mock_track:
                mock_track.return_value = {
                    "daw_command": "track(bass, serum)",
                    "result": {"id": "track_1", "name": "bass", "instrument": "serum"},
                    "context": {"tracks": {"bass": "track_1"}}
                }
                
                # Mock effect agent
                with patch.object(pipeline.agents['effect'], 'execute') as mock_effect:
                    mock_effect.return_value = {
                        "daw_command": "effect(bass, compressor, ratio=4)",
                        "result": {"id": "effect_1", "type": "compressor"},
                        "context": {"tracks": {"bass": "track_1"}}
                    }
                    
                    # Mock volume agent
                    with patch.object(pipeline.agents['volume'], 'execute') as mock_volume:
                        mock_volume.return_value = {
                            "daw_command": "volume(bass, -6)",
                            "result": {"id": "volume_1", "value": -6},
                            "context": {"tracks": {"bass": "track_1"}}
                        }
                        
                        result = pipeline.process_prompt(
                            "create a bass track with serum, add compressor with 4:1 ratio, and set volume to -6dB"
                        )
                        
                        expected_commands = [
                            "track(bass, serum)",
                            "effect(bass, compressor, ratio=4)",
                            "volume(bass, -6)"
                        ]
                        
                        assert result["daw_commands"] == expected_commands 