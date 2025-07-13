"""
Simple integration tests for quick verification.
"""

import os
import pytest
from magda.pipeline import MAGDAPipeline


class TestSimpleIntegration:
    """Simple integration tests with real API calls."""
    
    @pytest.fixture
    def pipeline(self):
        """Create a pipeline instance."""
        api_key = os.getenv("OPENAI_API_KEY")
        if not api_key:
            pytest.skip("OPENAI_API_KEY not set")
        return MAGDAPipeline()
    
    def test_basic_track_creation(self, pipeline):
        """Test basic track creation."""
        prompt = "Create a new track called 'Test Guitar'"
        
        print(f"\nTesting: {prompt}")
        result = pipeline.process_prompt(prompt)
        
        assert result is not None
        assert "operations" in result
        assert "daw_commands" in result
        assert len(result["operations"]) > 0
        print(f"✓ Track creation passed: {result}")
    
    def test_volume_adjustment(self, pipeline):
        """Test volume adjustment."""
        prompt = "Set the volume of track 'Test Guitar' to -3dB"
        
        print(f"\nTesting: {prompt}")
        result = pipeline.process_prompt(prompt)
        
        assert result is not None
        assert "operations" in result
        assert "daw_commands" in result
        assert len(result["operations"]) > 0
        print(f"✓ Volume adjustment passed: {result}")
    
    def test_effect_addition(self, pipeline):
        """Test effect addition."""
        prompt = "Add reverb to the 'Test Guitar' track"
        
        print(f"\nTesting: {prompt}")
        result = pipeline.process_prompt(prompt)
        
        assert result is not None
        assert "operations" in result
        assert "daw_commands" in result
        assert len(result["operations"]) > 0
        print(f"✓ Effect addition passed: {result}")
    
    def test_spanish_prompt(self, pipeline):
        """Test Spanish prompt."""
        prompt = "Crea una nueva pista llamada 'Piano Español'"
        
        print(f"\nTesting: {prompt}")
        result = pipeline.process_prompt(prompt)
        
        assert result is not None
        assert "operations" in result
        assert "daw_commands" in result
        assert len(result["operations"]) > 0
        print(f"✓ Spanish prompt passed: {result}")
    
    def test_french_prompt(self, pipeline):
        """Test French prompt."""
        prompt = "Ajoute une piste audio nommée 'Basse Française'"
        
        print(f"\nTesting: {prompt}")
        result = pipeline.process_prompt(prompt)
        
        assert result is not None
        assert "operations" in result
        assert "daw_commands" in result
        assert len(result["operations"]) > 0
        print(f"✓ French prompt passed: {result}")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"]) 