# MAGDA Action Design: REAPER API Translation

This document defines how actions are translated into REAPER API calls.

## Example 1: Add Track with Name

### User Request
"Add a track called 'Drums'"

### Action JSON
```json
{
  "action": "create_track",
  "name": "Drums"
}
```

### REAPER API Calls
1. `InsertTrackInProject(proj, index, flags)`
   - `proj`: `nullptr` (current project)
   - `index`: `GetNumTracks()` (append at end)
   - `flags`: `1` (default envelopes/FX)

2. `GetTrack(proj, index)` - Get the newly created track

3. `GetSetMediaTrackInfo(track, "P_NAME", name, nullptr)` - Set track name

### Implementation Status
✅ Already implemented in `CreateTrack()`

---

## Example 2: Add Track with Instrument and Name

### User Request
"Add a track with a piano instrument called 'Piano'"

### Action JSON Options

**Option A: Single action with instrument field**
```json
{
  "action": "create_track",
  "name": "Piano",
  "instrument": "VST3:ReaSynth (Cockos)"
}
```

**Option B: Separate actions (more flexible)**
```json
{
  "actions": [
    {
      "action": "create_track",
      "name": "Piano"
    },
    {
      "action": "add_track_fx",
      "track": "0",  // Will be the newly created track
      "fx_name": "VST3:ReaSynth (Cockos)"
    }
  ]
}
```

**Option C: Combined action (recommended)**
```json
{
  "action": "create_track",
  "name": "Piano",
  "instrument": "VST3:ReaSynth (Cockos)"
}
```

### REAPER API Calls
1. `InsertTrackInProject(proj, index, flags)` - Create track
2. `GetTrack(proj, index)` - Get track handle
3. `GetSetMediaTrackInfo(track, "P_NAME", name, nullptr)` - Set name
4. `TrackFX_AddByName(track, fxname, recFX, instantiate)` - Add instrument
   - `track`: Track handle
   - `fxname`: `"VST3:ReaSynth (Cockos)"` or instrument name
   - `recFX`: `false` (track FX, not input FX)
   - `instantiate`: `-1` (always create new instance)

### Implementation Needed
- Extend `CreateTrack()` to accept optional `instrument` field
- Add `TrackFX_AddByName` call after track creation

---

## Example 3: Add Track with Instrument and Clip at Bar 17

### User Request
"Add a track with a bass instrument and add a clip at bar 17"

### Action JSON

**Option A: Single combined action**
```json
{
  "action": "create_track_with_clip",
  "name": "Bass",
  "instrument": "VST3:ReaSynth (Cockos)",
  "clip": {
    "bar": 17,
    "length_bars": 4
  }
}
```

**Option B: Multiple actions (recommended - more flexible)**
```json
{
  "actions": [
    {
      "action": "create_track",
      "name": "Bass",
      "instrument": "VST3:ReaSynth (Cockos)"
    },
    {
      "action": "create_clip",
      "track": "0",  // Reference to newly created track
      "bar": 17,
      "length_bars": 4
    }
  ]
}
```

**Option C: Using time instead of bars**
```json
{
  "actions": [
    {
      "action": "create_track",
      "name": "Bass",
      "instrument": "VST3:ReaSynth (Cockos)"
    },
    {
      "action": "create_clip",
      "track": "0",
      "position": "64.0",  // Time in seconds (calculated from bar 17)
      "length": "16.0"     // Length in seconds (4 bars)
    }
  ]
}
```

### REAPER API Calls

#### For Track Creation:
1. `InsertTrackInProject(proj, index, flags)`
2. `GetTrack(proj, index)`
3. `GetSetMediaTrackInfo(track, "P_NAME", name, nullptr)`
4. `TrackFX_AddByName(track, instrument, false, -1)`

#### For Clip Creation:
1. Convert bar to time:
   - `TimeMap_GetMeasureInfo(proj, measure, ...)` - Get QN position for bar 17
   - `TimeMap2_QNToTime(proj, qn)` - Convert QN to time in seconds
   - Or: `TimeMap2_QNToTime(proj, (bar - 1) * 4.0)` if 4/4 time

2. Create clip:
   - `GetTrack(proj, track_index)` - Get track
   - `AddMediaItemToTrack(track)` - Create media item
   - `SetMediaItemPosition(item, position, false)` - Set position
   - `SetMediaItemLength(item, length, false)` - Set length
   - `UpdateArrange()` - Refresh UI

### Implementation Needed

1. **Bar to Time Conversion:**
   - Add helper function: `BarToTime(int bar, double* time_out)`
   - Uses `TimeMap_GetMeasureInfo()` to get time signature and QN
   - Uses `TimeMap2_QNToTime()` to convert QN to seconds

2. **Extend `CreateClip()`:**
   - Accept `bar` and `length_bars` fields in addition to `position` and `length`
   - Convert bars to time using helper function

3. **Extend `CreateTrack()`:**
   - Accept optional `instrument` field
   - Add FX after track creation

---

## Recommended Action Format

### `create_track` (Extended)
```json
{
  "action": "create_track",
  "index": 1,              // Optional: track index (defaults to end)
  "name": "Drums",         // Optional: track name
  "instrument": "VST3:ReaSynth (Cockos)"  // Optional: instrument/VST name
}
```

### `create_clip` (Extended)
```json
{
  "action": "create_clip",
  "track": "0",            // Required: track index
  "position": "64.0",     // Optional: position in seconds (if bar not provided)
  "length": "16.0",       // Optional: length in seconds (if length_bars not provided)
  "bar": 17,              // Optional: bar number (1-based, if position not provided)
  "length_bars": 4        // Optional: length in bars (if length not provided)
}
```

**Priority:**
- If `bar` is provided, use it (convert to time)
- If `position` is provided, use it directly
- If both provided, `bar` takes precedence
- Same logic for `length_bars` vs `length`

---

## Implementation Plan

1. ✅ **Basic track creation** - Already done
2. ⏳ **Add instrument support to `create_track`**
   - Add `instrument` field parsing
   - Call `TrackFX_AddByName` after track creation
3. ⏳ **Add bar/time conversion helper**
   - `BarToTime(int bar, double* time_out)`
   - `BarsToTime(int bars, double* time_out)`
4. ⏳ **Extend `create_clip` to support bars**
   - Parse `bar` and `length_bars` fields
   - Convert to time before creating clip
5. ⏳ **Update API format documentation**

---

## Notes

- **Instrument Names:** REAPER uses format like `"VST3:Plugin Name (Vendor)"` or just `"Plugin Name"`. The backend should provide the exact name as it appears in REAPER's FX browser.

- **Bar Numbers:** Bars are 1-based in user-facing interfaces (bar 1, bar 17, etc.), but we may need to convert to 0-based for some API calls.

- **Time Signature:** We assume 4/4 time for bar calculations unless we query the actual time signature from the project.

- **Track References:** When using multiple actions, the backend needs to know the track index. Options:
  - Use relative references: `"track": "last"` or `"track": "-1"` for last created track
  - Return track index from `create_track` and use it in subsequent actions
  - Use track name: `"track": "Piano"` (requires name lookup)
