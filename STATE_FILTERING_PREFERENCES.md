# State Filtering Preferences

## Overview

To optimize token usage and focus the LLM's attention, we can filter the state based on user preferences. This reduces payload size and helps the LLM focus on relevant information.

## Preference Options

### Filter Modes

1. **`all`** (default)
   - Send all tracks and all clips
   - Best for: General use, when working across multiple tracks
   - Token cost: Highest

2. **`selected_tracks_only`**
   - Send only tracks that are currently selected
   - Include all clips on selected tracks
   - Best for: Working on specific tracks
   - Token cost: Medium

3. **`selected_track_clips_only`**
   - Send only selected tracks
   - Include only clips that are selected on those tracks
   - Best for: Working on specific clips
   - Token cost: Lowest

4. **`selected_clips_only`**
   - Send all tracks (for context)
   - Include only selected clips (across all tracks)
   - Best for: Working with specific clips across multiple tracks
   - Token cost: Low-Medium

### Additional Options

- **`max_clips_per_track`**: Limit number of clips per track (e.g., 10)
  - Useful for tracks with many clips
  - Could show most recent or most relevant clips

- **`include_empty_tracks`**: Whether to include tracks with no clips
  - Default: true (for context)
  - Set to false to reduce noise

- **`include_midi_data`**: Whether to include MIDI note data in clips
  - Default: false (MIDI data can be large - 5KB+ per clip with many notes)
  - When true, only include for selected clips or limit note count
  - Recommended: Only for selected clips to keep state manageable

- **`include_audio_metadata`**: Whether to include audio analysis data
  - Default: false (adds ~200 bytes per clip)
  - When true, include RMS, peak, spectral data, etc.
  - Recommended: Only for selected clips

- **`max_midi_notes_per_clip`**: Limit MIDI notes if including MIDI data
  - Default: 100 notes
  - Prevents single complex clip from bloating state
  - Could prioritize notes in time selection or first N bars

## Implementation

### Storage

Store preferences in REAPER extension config:
- Registry key: `HKEY_CURRENT_USER\Software\REAPER\MAGDA\StateFilter`
- Or: Config file in REAPER resource path
- Or: Extension settings dialog

### API

```cpp
// In magda_state.h
enum class StateFilterMode {
    All,                    // Send everything
    SelectedTracksOnly,     // Only selected tracks + all their clips
    SelectedTrackClipsOnly, // Only selected tracks + only selected clips
    SelectedClipsOnly       // All tracks + only selected clips
};

struct StateFilterPreferences {
    StateFilterMode mode = StateFilterMode::All;
    int maxClipsPerTrack = -1;  // -1 = unlimited
    bool includeEmptyTracks = true;
};

// Load preferences
StateFilterPreferences LoadStateFilterPreferences();

// Apply filtering to state
void ApplyStateFilter(WDL_FastString &json, const StateFilterPreferences &prefs);
```

### Usage

```cpp
// In GetStateSnapshot()
StateFilterPreferences prefs = LoadStateFilterPreferences();
GetTracksInfo(json, prefs);  // Pass preferences to filtering
```

## UI Integration

Add to MAGDA settings dialog:
- Dropdown: "State Filter Mode"
  - All tracks and clips
  - Selected tracks only
  - Selected tracks + selected clips only
  - Selected clips only (all tracks)
- Checkbox: "Include empty tracks"
- Input: "Max clips per track" (optional)

## Benefits

1. **Reduced Token Usage**: 50-90% reduction depending on mode
2. **Faster Processing**: Less data for LLM to parse
3. **Better Focus**: LLM focuses on relevant information
4. **User Control**: Users can optimize for their workflow

## Examples

### Before (All Mode)
```json
{
  "tracks": [
    {"index": 0, "name": "Drums", "clips": [10 clips]},
    {"index": 1, "name": "Bass", "clips": [8 clips]},
    {"index": 2, "name": "Guitar", "clips": [12 clips]},
    {"index": 3, "name": "Keys", "clips": [5 clips]}
  ]
}
```
Size: ~5KB, ~1200 tokens

### After (Selected Tracks Only - Track 1 selected)
```json
{
  "tracks": [
    {"index": 1, "name": "Bass", "clips": [8 clips]}
  ]
}
```
Size: ~1KB, ~250 tokens (79% reduction)

### After (Selected Track Clips Only - Track 1, clips 2,3 selected)
```json
{
  "tracks": [
    {"index": 1, "name": "Bass", "clips": [
      {"index": 2, ...},
      {"index": 3, ...}
    ]}
  ]
}
```
Size: ~0.3KB, ~75 tokens (94% reduction)

## Migration

- Default to `all` mode for backward compatibility
- Add UI to change preference
- Document in user guide
