# Refactoring Action Names - Proposal

## Overview

This document proposes a refactoring to simplify and consolidate REAPER action names by:
1. Removing the `set_` prefix from action names
2. Consolidating multiple property setters into unified `set_track` and `set_clip` actions

## Current State

### Track Property Actions (6 separate actions)
- `set_track_name` - Set track name
- `set_track_volume` - Set track volume in dB
- `set_track_pan` - Set track pan (-1.0 to 1.0)
- `set_track_mute` - Mute/unmute track
- `set_track_solo` - Solo/unsolo track
- `set_track_selected` - Select/deselect track (also aliased as `select_track`)

### Clip Property Actions (4 separate actions)
- `set_clip_selected` - Select/deselect clip (also aliased as `select_clip`)
- `set_clip_name` - Set clip name
- `set_clip_color` - Set clip color
- `set_clip_position` - Move clip position

### Other Actions (no change needed)
- `create_track` - Create new track
- `create_clip` - Create clip at time position
- `create_clip_at_bar` - Create clip at bar number
- `add_track_fx` / `add_instrument` - Add FX/instrument
- `delete_track` / `remove_track` - Delete track
- `delete_clip` / `remove_clip` - Delete clip

## Proposed Changes

### Option 1: Unified Actions with Multiple Parameters (Recommended)

Consolidate all property setters into two unified actions that accept multiple optional parameters.

#### New Action: `set_track`

**Format:**
```json
{
  "action": "set_track",
  "track": 0,
  "name": "Drums",           // optional
  "volume_db": -6.0,         // optional
  "pan": 0.5,                // optional
  "mute": false,             // optional
  "solo": true,              // optional
  "selected": true           // optional
}
```

**Benefits:**
- Single action type for all track property updates
- Atomic updates (all properties set together)
- More efficient (single REAPER API call per track)
- Matches REST API patterns
- Aligns with DSL chaining (`track().set_volume().set_name()`)

**Example Use Cases:**
```json
// Set only name
{"action": "set_track", "track": 0, "name": "Drums"}

// Set multiple properties at once
{"action": "set_track", "track": 0, "name": "Drums", "volume_db": -6.0, "mute": false}

// Set all properties
{"action": "set_track", "track": 0, "name": "Drums", "volume_db": -6.0, "pan": 0.5, "mute": false, "solo": true, "selected": true}
```

#### New Action: `set_clip`

**Format:**
```json
{
  "action": "set_clip",
  "track": 0,
  "clip": 1,                 // optional: clip index
  "position": 5.0,           // optional: clip position (seconds) - for identification or movement
  "bar": 2,                  // optional: clip bar number - for identification or movement
  "name": "Intro",           // optional
  "color": "#ff0000",        // optional
  "length": 2.0,             // optional: clip length (seconds)
  "selected": true           // optional
}
```

**Note:** For clip identification, use one of: `clip` (index), `position` (seconds), or `bar` (bar number).

**Example Use Cases:**
```json
// Rename clip by index
{"action": "set_clip", "track": 0, "clip": 1, "name": "Intro"}

// Move and rename clip by position
{"action": "set_clip", "track": 0, "position": 5.0, "name": "Intro", "position": 8.0}

// Set multiple properties
{"action": "set_clip", "track": 0, "clip": 1, "name": "Intro", "color": "#ff0000", "selected": true}
```

### Option 2: Remove `set_` Prefix Only

Keep separate actions but remove the `set_` prefix:

- `set_track_name` → `track_name`
- `set_track_volume` → `track_volume`
- `set_track_pan` → `track_pan`
- `set_track_mute` → `track_mute`
- `set_track_solo` → `track_solo`
- `set_track_selected` → `track_selected` (keep `select_track` alias)
- `set_clip_selected` → `clip_selected` (keep `select_clip` alias)
- `set_clip_name` → `clip_name`
- `set_clip_color` → `clip_color`
- `set_clip_position` → `clip_position`

**Benefits:**
- Cleaner action names
- Less verbose
- Still allows granular control

**Drawbacks:**
- Still many action types
- No atomic updates
- Multiple API calls for multiple properties

## Recommendation

**Option 1 (Unified Actions)** is recommended because:
1. **Efficiency**: Single REAPER API call per track/clip for multiple properties
2. **Atomicity**: All properties updated together (no partial updates)
3. **Simplicity**: Fewer action types to maintain
4. **Scalability**: Easy to add new properties without new action types
5. **REST Alignment**: Matches common REST API patterns

## Implementation Plan

### Phase 1: Add New Unified Actions (Backward Compatible)

1. **C++ Implementation** (`magda-reaper/src/magda_actions.cpp`):
   - Add `SetTrackProperties()` method that accepts all optional parameters
   - Add `SetClipProperties()` method that accepts all optional parameters
   - Add handlers for `set_track` and `set_clip` actions
   - Keep old action handlers for backward compatibility

2. **Go DSL Parser** (`magda-agents-go/agents/daw/dsl_parser_functional.go`):
   - Update action generation to use unified `set_track` and `set_clip` actions
   - When multiple properties are set in a chain, combine into single action
   - Example: `track().set_volume(-6.0).set_name("Drums")` → single `set_track` action

3. **Tests**:
   - Add tests for unified actions
   - Keep existing tests for old actions (backward compatibility)

### Phase 2: Update Documentation and Prompts

1. **Documentation**:
   - Update `MISSING_ACTIONS.md` with new action format
   - Update prompt examples in `magda-agents-go/prompt/magda.go`
   - Update API documentation

2. **Prompts**:
   - Update examples to show unified action format
   - Guide LLM to generate unified actions when multiple properties are set

### Phase 3: Migration (Optional - Deprecate Old Actions)

1. **Deprecation Period**:
   - Keep old actions working but log deprecation warnings
   - Update all internal code to use new actions
   - Update all tests to use new actions

2. **Remove Old Actions** (Future):
   - Remove old action handlers after deprecation period
   - Remove old action names from documentation

## Breaking Changes

### If We Remove Old Actions Immediately

**Breaking Changes:**
- All existing code using `set_track_name`, `set_track_volume`, etc. will break
- All tests expecting old action names will fail
- LLM-generated DSL will need to be updated

**Migration Required:**
- Update all Go code generating actions
- Update all tests
- Update all documentation
- Update prompts

### If We Keep Backward Compatibility

**No Breaking Changes:**
- Old actions continue to work
- New actions available alongside old ones
- Gradual migration possible

**Recommended Approach:**
- Implement new actions alongside old ones
- Update internal code to use new actions
- Keep old actions for backward compatibility
- Document both formats

## Code Changes Required

### C++ Changes (`magda-reaper/src/magda_actions.cpp`)

```cpp
// New unified method
static bool SetTrackProperties(int track_index,
                                const char *name,
                                const char *volume_db_str,
                                const char *pan_str,
                                const char *mute_str,
                                const char *solo_str,
                                const char *selected_str,
                                WDL_FastString &error_msg);

// New action handler
else if (strcmp(action_type, "set_track") == 0) {
  const char *track_str = action->get_string_by_name("track", true);
  if (!track_str) {
    error_msg.Set("Missing 'track' field");
    return false;
  }
  int track_index = atoi(track_str);

  // Get all optional parameters
  const char *name = action->get_string_by_name("name");
  const char *volume_db_str = action->get_string_by_name("volume_db", true);
  const char *pan_str = action->get_string_by_name("pan", true);
  const char *mute_str = action->get_string_by_name("mute", true);
  const char *solo_str = action->get_string_by_name("solo", true);
  const char *selected_str = action->get_string_by_name("selected", true);

  if (SetTrackProperties(track_index, name, volume_db_str, pan_str,
                         mute_str, solo_str, selected_str, error_msg)) {
    result.Append("{\"action\":\"set_track\",\"success\":true}");
    return true;
  }
  return false;
}
```

### Go Changes (`magda-agents-go/agents/daw/dsl_parser_functional.go`)

**Current (generates multiple actions):**
```go
// track().set_volume(-6.0).set_name("Drums")
// Generates:
// {"action": "set_track_volume", "track": 0, "volume_db": -6.0}
// {"action": "set_track_name", "track": 0, "name": "Drums"}
```

**Proposed (generates single action):**
```go
// track().set_volume(-6.0).set_name("Drums")
// Generates:
// {"action": "set_track", "track": 0, "volume_db": -6.0, "name": "Drums"}
```

**Implementation:**
- Track pending property updates during method chaining
- When chain completes, generate single `set_track` action with all properties
- For filtered collections, generate one `set_track` action per item with all properties

## Examples

### Before (Current)
```json
[
  {"action": "set_track_name", "track": 0, "name": "Drums"},
  {"action": "set_track_volume", "track": 0, "volume_db": -6.0},
  {"action": "set_track_mute", "track": 0, "mute": false}
]
```

### After (Proposed)
```json
[
  {"action": "set_track", "track": 0, "name": "Drums", "volume_db": -6.0, "mute": false}
]
```

### DSL Example

**Current DSL:**
```
track(id=0).set_name(name="Drums").set_volume(volume_db=-6.0).set_mute(mute=false)
```
Generates 3 separate actions.

**Proposed DSL (same syntax, different output):**
```
track(id=0).set_name(name="Drums").set_volume(volume_db=-6.0).set_mute(mute=false)
```
Generates 1 unified action.

### Filtered Collections

**Current:**
```
filter(tracks, track.muted == true).set_name(name="Muted").set_volume(volume_db=-6.0)
```
Generates 2 actions per filtered track (6 actions for 3 tracks).

**Proposed:**
```
filter(tracks, track.muted == true).set_name(name="Muted").set_volume(volume_db=-6.0)
```
Generates 1 action per filtered track (3 actions for 3 tracks).

## Testing Strategy

1. **Unit Tests**:
   - Test `SetTrackProperties()` with all parameter combinations
   - Test `SetClipProperties()` with all parameter combinations
   - Test backward compatibility with old action names

2. **Integration Tests**:
   - Test unified actions in real REAPER environment
   - Test DSL parser generates unified actions correctly
   - Test filtered collections with unified actions

3. **Backward Compatibility Tests**:
   - Verify old action names still work
   - Verify old action format still accepted

## Timeline

1. **Week 1**: Implement unified actions in C++ (backward compatible)
2. **Week 2**: Update Go DSL parser to generate unified actions
3. **Week 3**: Update tests and documentation
4. **Week 4**: Update prompts and verify LLM generates correct format
5. **Future**: Consider deprecating old actions after migration period

## Questions to Resolve

1. **Backward Compatibility**: Keep old actions indefinitely or deprecate after migration?
2. **DSL Syntax**: Keep current DSL syntax (`.set_name().set_volume()`) or change?
3. **Migration Strategy**: Big bang or gradual migration?
4. **Documentation**: Document both formats or only new format?

## Decision

**Status**: Proposal - Awaiting approval

**Recommended Approach**:
- Implement Option 1 (Unified Actions)
- Keep backward compatibility initially
- Update internal code to use new format
- Document both formats
- Consider deprecation after migration period

---

**Created**: 2025-12-10
**Author**: MAGDA Development Team
**Status**: Proposal - Ready for Review
