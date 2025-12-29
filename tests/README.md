# MAGDA REAPER Tests

This directory contains both unit tests and integration tests for the MAGDA REAPER plugin.

## Test Types

### Unit Tests (`tests/unit/`)

Unit tests run **without REAPER** and can be executed in CI environments (GitHub Actions).

**What's tested:**
- DSL Tokenizer - parsing DSL input into tokens
- Params class - parameter map functionality
- JSON parsing - WDL JSON parser for API responses

**Running unit tests:**

```bash
cd tests/unit
cmake -B build .
cmake --build build
ctest --test-dir build --output-on-failure
```

Or use the convenience script:
```bash
./tests/run_unit_tests.sh
```

### Integration Tests (`tests/*.lua`)

Integration tests run **inside REAPER** and test actual plugin functionality.

**What's tested:**
- REAPER API interactions
- MAGDA action execution
- DSL interpretation with real tracks/clips

**Running integration tests:**

```bash
./tests/run_tests.sh
```

This will:
1. Open REAPER (window appears on macOS/Windows)
2. Execute test Lua scripts
3. Close REAPER when tests complete

## CI/CD

Unit tests run automatically on every push/PR via GitHub Actions:
- Tests run on Ubuntu and macOS
- No REAPER required
- Fast feedback (~1-2 minutes)

Integration tests are intended for local development only (require REAPER installation).

## Writing Tests

### Unit Tests

Add new test files to `tests/unit/`:

```cpp
#include <gtest/gtest.h>

TEST(MyFeature, DoesExpectedBehavior) {
    // Arrange
    MyClass obj;

    // Act
    auto result = obj.DoSomething();

    // Assert
    EXPECT_EQ(result, expected);
}
```

Update `tests/unit/CMakeLists.txt` to include new test executables.

### Integration Tests

Add test functions to `magda_integration_test.lua`:

```lua
function test_my_new_action()
  log("Testing my_new_action...")

  -- Setup
  local track = reaper.GetTrack(0, 0)

  -- Execute MAGDA action via plugin
  local json = '{"action": "my_action", "params": {...}}'
  reaper.SetProjExtState(0, "MAGDA_TEST", "ACTION_JSON", json)
  reaper.Main_OnCommand(cmd_id, 0)

  -- Read result
  local _, result = reaper.GetProjExtState(0, "MAGDA_TEST", "RESULT")

  -- Assert
  assert(result == "OK", "Expected OK")
end
```

## Test Coverage Goals

- [x] DSL Tokenizer
- [x] Params class
- [x] JSON parsing
- [ ] DSL Interpreter (with REAPER mocks)
- [ ] API client (with HTTP mocks)
- [ ] OpenAI streaming parser
