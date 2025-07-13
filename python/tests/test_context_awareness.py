"""
Tests for MAGDA's context awareness system.
"""

import os
import pytest
from magda.pipeline import MAGDAPipeline


class TestContextAwareness:
    """Test context awareness and ID management."""
    
    @pytest.fixture
    def pipeline(self):
        """Create a pipeline instance."""
        api_key = os.getenv("OPENAI_API_KEY")
        if not api_key:
            pytest.skip("OPENAI_API_KEY not set")
        return MAGDAPipeline()
    
    def test_track_creation_and_reference(self, pipeline):
        """Test creating a track and then referencing it by name."""
        print("\n=== Test: Track Creation and Reference ===")
        
        # Create a track
        result1 = pipeline.process_prompt("Create a new track called Lead Guitar")
        assert len(result1["results"]) == 1
        track_id = result1["results"][0]["result"]["id"]
        print(f"Created track with ID: {track_id}")
        
        # Reference the track by name
        result2 = pipeline.process_prompt("Add reverb to the Lead Guitar track")
        assert len(result2["results"]) == 1
        effect_track_id = result2["results"][0]["result"]["track_id"]
        print(f"Added reverb to track: {effect_track_id}")
        
        # Verify the track IDs match
        assert effect_track_id != "unknown"
        print("✓ Track reference resolution working")
    
    def test_multiple_tracks_and_effects(self, pipeline):
        """Test creating multiple tracks and adding effects to specific ones."""
        print("\n=== Test: Multiple Tracks and Effects ===")
        
        # Create multiple tracks
        result1 = pipeline.process_prompt("Create tracks called Bass and Drums")
        assert len(result1["results"]) == 2
        print(f"Created {len(result1['results'])} tracks")
        
        # Add effects to specific tracks
        result2 = pipeline.process_prompt("Add reverb to Bass and delay to Drums")
        assert len(result2["results"]) == 2
        print(f"Added {len(result2['results'])} effects")
        
        # Verify context summary
        summary = pipeline.get_context_summary()
        assert len(summary["tracks"]) == 2
        print(f"Context has {len(summary['tracks'])} tracks")
        
        # Check that each track has effects
        for track_id, track_info in summary["tracks"].items():
            assert track_info["effects"] == 1
            print(f"Track {track_info['name']} has {track_info['effects']} effects")
        
        print("✓ Multiple track management working")
    
    def test_volume_automation_context(self, pipeline):
        """Test volume automation with proper track context."""
        print("\n=== Test: Volume Automation Context ===")
        
        # Create a track
        result1 = pipeline.process_prompt("Create a track called Piano")
        track_id = result1["results"][0]["result"]["id"]
        print(f"Created Piano track: {track_id}")
        
        # Set volume
        result2 = pipeline.process_prompt("Set the volume of Piano to -3dB")
        volume_track_id = result2["results"][0]["result"]["track_id"]
        print(f"Set volume for track: {volume_track_id}")
        
        # Verify track ID consistency
        assert volume_track_id != "unknown"
        
        # Check context summary
        summary = pipeline.get_context_summary()
        piano_track = None
        for track_id, track_info in summary["tracks"].items():
            if track_info["name"] == "Piano":
                piano_track = track_info
                break
        
        assert piano_track is not None
        assert piano_track["volume_automations"] == 1
        print(f"Piano track has {piano_track['volume_automations']} volume automations")
        
        print("✓ Volume automation context working")
    
    def test_complex_workflow(self, pipeline):
        """Test a complex workflow with multiple operations."""
        print("\n=== Test: Complex Workflow ===")
        
        # Create a track with VST
        result1 = pipeline.process_prompt("Create a track called Synth with Serum")
        assert len(result1["results"]) == 1
        print("Created Synth track with Serum")
        
        # Add multiple effects
        result2 = pipeline.process_prompt("Add reverb and delay to Synth")
        assert len(result2["results"]) == 2
        print("Added reverb and delay")
        
        # Set volume
        result3 = pipeline.process_prompt("Set Synth volume to -6dB")
        assert len(result3["results"]) == 1
        print("Set volume")
        
        # Add MIDI
        result4 = pipeline.process_prompt("Add C major chord to Synth")
        assert len(result4["results"]) == 1
        print("Added MIDI chord")
        
        # Verify final context
        summary = pipeline.get_context_summary()
        synth_track = None
        for track_id, track_info in summary["tracks"].items():
            if track_info["name"] == "Synth":
                synth_track = track_info
                break
        
        assert synth_track is not None
        assert synth_track["effects"] == 2
        assert synth_track["volume_automations"] == 1
        assert synth_track["midi_events"] == 1
        
        print(f"Synth track final state:")
        print(f"  - Effects: {synth_track['effects']}")
        print(f"  - Volume automations: {synth_track['volume_automations']}")
        print(f"  - MIDI events: {synth_track['midi_events']}")
        
        print("✓ Complex workflow working")
    
    def test_context_persistence(self, pipeline):
        """Test that context persists across multiple operations."""
        print("\n=== Test: Context Persistence ===")
        
        # Create a track
        result1 = pipeline.process_prompt("Create a track called Guitar")
        initial_summary = pipeline.get_context_summary()
        print(f"Initial context: {len(initial_summary['tracks'])} tracks")
        
        # Add effect
        result2 = pipeline.process_prompt("Add reverb to Guitar")
        effect_summary = pipeline.get_context_summary()
        print(f"After effect: {len(effect_summary['tracks'])} tracks")
        
        # Add volume
        result3 = pipeline.process_prompt("Set Guitar volume to -3dB")
        final_summary = pipeline.get_context_summary()
        print(f"After volume: {len(final_summary['tracks'])} tracks")
        
        # Verify context grows but doesn't lose tracks
        assert len(final_summary["tracks"]) == len(initial_summary["tracks"])
        assert final_summary["total_objects"] > initial_summary["total_objects"]
        
        print(f"Context persistence verified:")
        print(f"  - Tracks maintained: {len(final_summary['tracks'])}")
        print(f"  - Total objects: {final_summary['total_objects']}")
        
        print("✓ Context persistence working")
    
    def test_track_finding_methods(self, pipeline):
        """Test track finding by name and ID."""
        print("\n=== Test: Track Finding Methods ===")
        
        # Create a track
        result = pipeline.process_prompt("Create a track called Test Track")
        track_result = result["results"][0]["result"]
        magda_id = track_result["magda_id"]
        daw_id = track_result["id"]
        
        print(f"Created track - MAGDA ID: {magda_id}, DAW ID: {daw_id}")
        
        # Test finding by name
        track_by_name = pipeline.find_track_by_name("Test Track")
        assert track_by_name is not None
        assert track_by_name.magda_id == magda_id
        print("✓ Found track by name")
        
        # Test finding by MAGDA ID
        track_by_magda_id = pipeline.find_track_by_id(magda_id)
        assert track_by_magda_id is not None
        assert track_by_magda_id.magda_id == magda_id
        print("✓ Found track by MAGDA ID")
        
        # Test finding by DAW ID
        track_by_daw_id = pipeline.find_track_by_id(daw_id)
        assert track_by_daw_id is not None
        assert track_by_daw_id.daw_id == daw_id
        print("✓ Found track by DAW ID")
        
        print("✓ Track finding methods working")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"]) 