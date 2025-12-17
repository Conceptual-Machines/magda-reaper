# MAGDA REAPER Integration Tests

Integration tests for the MAGDA REAPER plugin using REAPER's CLI and Lua scripting.

## Overview

These tests run REAPER in headless mode and execute Lua scripts that test plugin functionality. This allows for automated testing without manual interaction.

## Running Tests

### Quick Start

```bash
cd magda-reaper
./tests/run_tests.sh
```

### Manual Execution

You can also run tests manually:

```bash
# Run a specific test script
/Applications/REAPER.app/Contents/MacOS/REAPER \
  -nosplash \
  -new \
  -ReaScript tests/integration_test.lua \
  -close:save:exit
```

## Test Structure

- `integration_test.lua` - Basic REAPER API tests (direct API calls)
- `magda_integration_test.lua` - **MAGDA action tests** (tests plugin functionality)
- `run_tests.sh` - Test runner script that handles setup and execution
- `test_projects/` - Directory for test project files (created automatically)

## Testing MAGDA Commands Headlessly

The plugin exposes a test command (`MAGDA: Test Execute Action`) that allows Lua scripts to execute MAGDA actions:

1. **Store action JSON** in project state: `reaper.SetProjExtState(0, "MAGDA_TEST", "ACTION_JSON", json)`
2. **Execute command**: `reaper.Main_OnCommand(cmd_id, 0)`
3. **Read result**: `reaper.GetProjExtState(0, "MAGDA_TEST", "RESULT")` or `"ERROR"`

This allows full headless testing of MAGDA functionality without UI interaction.

## Writing New Tests

Add test functions to `integration_test.lua`:

```lua
function test_my_new_action()
  log("Testing my_new_action...")

  -- Setup
  local track = reaper.GetTrack(0, 0)

  -- Execute action (via plugin or direct API)
  -- ...

  -- Assert
  assert(condition, "Test description")

  -- Cleanup
  -- ...
end
```

Then call it in the test runner section.

## REAPER CLI Options

Key options for headless testing:

- `-nosplash` - Don't show splash screen
- `-new` - Start with new project
- `-ReaScript script.lua` - Run Lua script
- `-close:save:exit` - Save and exit after script
- `-cfgfile file.ini` - Use alternate config (for isolated testing)

## Limitations

- **macOS/Windows**: REAPER will show a GUI window (no true headless mode on these platforms)
  - Tests still run automatically and REAPER closes when script completes
  - Window appears briefly during test execution
- **Linux**: Can run truly headless with custom libSwell build
- Some tests may require audio device (can be configured to use dummy driver)
- Plugin must be installed in UserPlugins directory

## Running Tests

The test script will:
1. Open REAPER (window will appear on macOS/Windows)
2. Execute test Lua scripts automatically
3. Close REAPER when tests complete (via `os.exit()` in scripts)

You can minimize the REAPER window - tests will still run in the background.

## Future Enhancements

- [ ] Test plugin API directly (not just REAPER API)
- [ ] Test MAGDA action execution end-to-end
- [ ] Test DSL parsing and execution
- [ ] Test HTTP client communication
- [ ] CI/CD integration (GitHub Actions)
