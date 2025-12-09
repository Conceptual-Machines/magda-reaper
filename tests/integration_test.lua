-- MAGDA REAPER Integration Tests
-- Run with: reaper -nosplash -new -ReaScript tests/integration_test.lua

local test_results = {}
local tests_passed = 0
local tests_failed = 0

function log(message)
  reaper.ShowConsoleMsg("[TEST] " .. message .. "\n")
end

function assert(condition, message)
  if condition then
    tests_passed = tests_passed + 1
    log("✓ PASS: " .. (message or "assertion"))
    return true
  else
    tests_failed = tests_failed + 1
    log("✗ FAIL: " .. (message or "assertion"))
    return false
  end
end

function test_create_track()
  log("Testing create_track action...")

  -- Get initial track count
  local initial_count = reaper.GetNumTracks()

  -- Create a track via MAGDA action (simulated)
  -- In real test, we'd call the plugin's action executor
  reaper.InsertTrackAtIndex(initial_count, false)

  local new_count = reaper.GetNumTracks()
  assert(new_count == initial_count + 1, "Track count increased after creation")

  -- Cleanup
  reaper.DeleteTrack(reaper.GetTrack(0, initial_count))
end

function test_set_track_name()
  log("Testing set_track_name action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Set track name
  local test_name = "Test Track " .. os.time()
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", test_name, true)

  -- Verify name was set
  local retrieved_name = reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "", false)
  assert(retrieved_name == test_name, "Track name was set correctly")

  -- Cleanup
  reaper.DeleteTrack(track)
end

function test_create_clip()
  log("Testing create_clip action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Create a media item (clip)
  local item = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemPosition(item, 0.0, false)
  reaper.SetMediaItemLength(item, 4.0, false)

  -- Verify clip was created
  local item_count = reaper.CountTrackMediaItems(track)
  assert(item_count == 1, "Clip was created on track")

  -- Cleanup
  reaper.DeleteTrack(track)
end

function test_set_track_volume()
  log("Testing set_track_volume action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Set volume to -6dB (0.5 linear)
  reaper.SetMediaTrackInfo_Value(track, "D_VOL", 0.5)

  -- Verify volume was set
  local volume = reaper.GetMediaTrackInfo_Value(track, "D_VOL")
  assert(math.abs(volume - 0.5) < 0.001, "Track volume was set correctly")

  -- Cleanup
  reaper.DeleteTrack(track)
end

function test_set_track_selected()
  log("Testing set_track_selected action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Select track
  reaper.SetTrackSelected(track, true)

  -- Verify selection
  local selected = reaper.IsTrackSelected(track)
  assert(selected == true, "Track was selected")

  -- Deselect
  reaper.SetTrackSelected(track, false)
  selected = reaper.IsTrackSelected(track)
  assert(selected == false, "Track was deselected")

  -- Cleanup
  reaper.DeleteTrack(track)
end

-- Run all tests
log("=" .. string.rep("=", 60))
log("Starting MAGDA REAPER Integration Tests")
log("=" .. string.rep("=", 60))

test_create_track()
test_set_track_name()
test_create_clip()
test_set_track_volume()
test_set_track_selected()

-- Print summary
log("=" .. string.rep("=", 60))
log(string.format("Tests completed: %d passed, %d failed", tests_passed, tests_failed))
log("=" .. string.rep("=", 60))

if tests_failed > 0 then
  reaper.ShowConsoleMsg("\n[TEST] Some tests failed!\n")
  os.exit(1)
else
  reaper.ShowConsoleMsg("\n[TEST] All tests passed!\n")
  os.exit(0)
end
