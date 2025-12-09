-- MAGDA REAPER Integration Tests - Testing MAGDA Actions
-- Run with: reaper -nosplash -new -ReaScript tests/magda_integration_test.lua

local test_results = {}
local tests_passed = 0
local tests_failed = 0

function log(message)
  reaper.ShowConsoleMsg("[MAGDA TEST] " .. message .. "\n")
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

-- Helper function to execute MAGDA action via plugin
function execute_magda_action(action_json)
  -- Store action JSON in project state
  reaper.SetProjExtState(0, "MAGDA_TEST", "ACTION_JSON", action_json)
  reaper.SetProjExtState(0, "MAGDA_TEST", "RESULT", "")
  reaper.SetProjExtState(0, "MAGDA_TEST", "ERROR", "")

  -- Execute the action via registered command
  local cmd_id = reaper.NamedCommandLookup("_MAGDA_TEST_EXECUTE_ACTION")
  if cmd_id == 0 then
    -- Try alternative lookup
    cmd_id = reaper.NamedCommandLookup("MAGDA: Test Execute Action")
  end

  if cmd_id == 0 then
    log("ERROR: MAGDA test command not found. Plugin may not be loaded.")
    return false, "Command not found"
  end

  -- Execute command
  reaper.Main_OnCommand(cmd_id, 0)

  -- Wait a tiny bit for execution
  reaper.defer(function() end)

  -- Read result from project state
  local result = reaper.GetProjExtState(0, "MAGDA_TEST", "RESULT")
  local error_msg = reaper.GetProjExtState(0, "MAGDA_TEST", "ERROR")

  if error_msg and error_msg ~= "" then
    return false, error_msg
  end

  return true, result
end

function test_magda_create_track()
  log("Testing MAGDA create_track action...")

  local initial_count = reaper.GetNumTracks()

  -- Execute MAGDA action
  local action_json = '{"action":"create_track","name":"MAGDA Test Track","index":-1}'
  local success, result = execute_magda_action(action_json)

  if not success then
    assert(false, "MAGDA create_track failed: " .. (result or "unknown error"))
    return
  end

  -- Verify track was created
  local new_count = reaper.GetNumTracks()
  assert(new_count == initial_count + 1, "Track count increased after MAGDA create_track")

  -- Verify track name
  if new_count > initial_count then
    local track = reaper.GetTrack(0, initial_count)
    local track_name = reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "", false)
    assert(track_name == "MAGDA Test Track", "Track name was set correctly")

    -- Cleanup
    reaper.DeleteTrack(track)
  end
end

function test_magda_set_track_volume()
  log("Testing MAGDA set_track_volume action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Execute MAGDA action to set volume to -6dB
  local action_json = string.format('{"action":"set_track_volume","track":%d,"volume_db":-6.0}', track_idx)
  local success, result = execute_magda_action(action_json)

  if not success then
    assert(false, "MAGDA set_track_volume failed: " .. (result or "unknown error"))
    reaper.DeleteTrack(track)
    return
  end

  -- Verify volume was set (-6dB = 0.5 linear)
  local volume = reaper.GetMediaTrackInfo_Value(track, "D_VOL")
  assert(math.abs(volume - 0.5) < 0.01, "Track volume was set to -6dB via MAGDA")

  -- Cleanup
  reaper.DeleteTrack(track)
end

function test_magda_create_clip()
  log("Testing MAGDA create_clip action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Execute MAGDA action to create clip
  local action_json = string.format('{"action":"create_clip","track":%d,"position":0.0,"length":4.0}', track_idx)
  local success, result = execute_magda_action(action_json)

  if not success then
    assert(false, "MAGDA create_clip failed: " .. (result or "unknown error"))
    reaper.DeleteTrack(track)
    return
  end

  -- Verify clip was created
  local item_count = reaper.CountTrackMediaItems(track)
  assert(item_count == 1, "Clip was created via MAGDA action")

  -- Cleanup
  reaper.DeleteTrack(track)
end

function test_magda_set_track_selected()
  log("Testing MAGDA set_track_selected action...")

  -- Create a test track
  local track_idx = reaper.GetNumTracks()
  reaper.InsertTrackAtIndex(track_idx, false)
  local track = reaper.GetTrack(0, track_idx)

  -- Execute MAGDA action to select track
  local action_json = string.format('{"action":"set_track_selected","track":%d,"selected":true}', track_idx)
  local success, result = execute_magda_action(action_json)

  if not success then
    assert(false, "MAGDA set_track_selected failed: " .. (result or "unknown error"))
    reaper.DeleteTrack(track)
    return
  end

  -- Verify selection
  local selected = reaper.IsTrackSelected(track)
  assert(selected == true, "Track was selected via MAGDA action")

  -- Cleanup
  reaper.DeleteTrack(track)
end

-- Run all tests
log("=" .. string.rep("=", 60))
log("Starting MAGDA REAPER Integration Tests")
log("=" .. string.rep("=", 60))

-- Check if plugin is loaded
local cmd_id = reaper.NamedCommandLookup("_MAGDA_TEST_EXECUTE_ACTION")
if cmd_id == 0 then
  cmd_id = reaper.NamedCommandLookup("MAGDA: Test Execute Action")
end

if cmd_id == 0 then
  log("ERROR: MAGDA plugin not loaded or test command not registered")
  log("Make sure the plugin is built and installed in UserPlugins directory")
  os.exit(1)
end

log("MAGDA plugin detected, running tests...")
log("")

test_magda_create_track()
test_magda_set_track_volume()
test_magda_create_clip()
test_magda_set_track_selected()

-- Print summary
log("=" .. string.rep("=", 60))
log(string.format("Tests completed: %d passed, %d failed", tests_passed, tests_failed))
log("=" .. string.rep("=", 60))

if tests_failed > 0 then
  reaper.ShowConsoleMsg("\n[MAGDA TEST] Some tests failed!\n")
  os.exit(1)
else
  reaper.ShowConsoleMsg("\n[MAGDA TEST] All tests passed!\n")
  os.exit(0)
end
