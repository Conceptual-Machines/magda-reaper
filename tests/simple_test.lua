-- Simple test to verify REAPER can run Lua scripts
reaper.ShowConsoleMsg("=== SIMPLE TEST STARTING ===\n")

-- Test basic REAPER API
local track_count = reaper.GetNumTracks()
reaper.ShowConsoleMsg(string.format("Current track count: %d\n", track_count))

-- Create a track
reaper.InsertTrackAtIndex(track_count, false)
local new_count = reaper.GetNumTracks()
reaper.ShowConsoleMsg(string.format("New track count: %d\n", new_count))

if new_count == track_count + 1 then
  reaper.ShowConsoleMsg("✓ TEST PASSED: Track created successfully\n")
  -- Cleanup
  local track = reaper.GetTrack(0, track_count)
  if track then
    reaper.DeleteTrack(track)
  end
else
  reaper.ShowConsoleMsg("✗ TEST FAILED: Track count mismatch\n")
end

reaper.ShowConsoleMsg("=== SIMPLE TEST COMPLETE ===\n")
reaper.ShowConsoleMsg("Exiting REAPER...\n")

-- Exit REAPER
os.exit(0)
