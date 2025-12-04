# Missing REAPER Actions - Implementation Wishlist

## Currently Implemented ✅

1. **create_track** - Create new track (with optional name/instrument)
2. **set_track_name** - Rename track
3. **set_track_volume** - Set track volume in dB
4. **set_track_pan** - Set track pan (-1.0 to 1.0)
5. **set_track_mute** - Mute/unmute track
6. **set_track_solo** - Solo/unsolo track
7. **add_instrument** - Add VSTi to track
8. **add_track_fx** - Add FX plugin to track
9. **create_clip** - Create clip at time position
10. **create_clip_at_bar** - Create clip at bar number

## High Priority Missing Actions 🔴

### Selection (Critical for functional methods)
- **set_track_selected** / **select_track**
  - Select/deselect a track
  - Required for: "select all tracks named X", functional operations
  - REAPER API: `SetTrackSelected(MediaTrack *track, bool selected)`
  - Status: State reads selection, but no action to set it

- **set_clip_selected** / **select_clip**
  - Select/deselect a media item/clip
  - Required for: "select all clips", bulk operations
  - REAPER API: `SetMediaItemSelected(MediaItem *item, bool selected)`
  - Status: Not implemented

### Deletion
- **delete_track**
  - Delete a track and all its contents
  - Required for: "delete track named X", cleanup operations
  - REAPER API: `DeleteTrack(MediaTrack *track)` or `RemoveTrack(MediaTrack *track)`

- **delete_clip** / **remove_clip**
  - Delete a media item/clip
  - Required for: "remove all clips from track", cleanup
  - REAPER API: `DeleteTrackMediaItem(MediaTrack *track, MediaItem *item)` or similar

### Clip Editing
- **set_clip_name**
  - Set name/label for a clip
  - Required for: organizing clips
  - REAPER API: `GetSetMediaItemInfo_String(MediaItem *item, "P_NAME", ...)`

- **set_clip_position** / **move_clip**
  - Move clip to different time position
  - Required for: "move clip to bar X"
  - REAPER API: `SetMediaItemPosition(MediaItem *item, double position, bool adjust_length)`

- **set_clip_length**
  - Change clip duration
  - Required for: "extend clip to 8 bars"
  - REAPER API: `SetMediaItemLength(MediaItem *item, double length, bool adjust_take_length)`

## Medium Priority Actions 🟡

### Track Reordering/Grouping
- **move_track** / **reorder_track**
  - Move track to different position in list
  - Required for: "move track 3 to position 1"
  - REAPER API: `ReorderSelectedTracks(int from_index, int to_index)` or track GUID manipulation

- **set_track_folder** / **group_tracks**
  - Create folder track or group tracks
  - Required for: organizing tracks
  - REAPER API: Track folder flags via `GetSetMediaTrackInfo`

### FX Management
- **remove_fx** / **delete_fx**
  - Remove FX from track
  - Required for: "remove all EQ from track"
  - REAPER API: `TrackFX_Delete(MediaTrack *track, int fx_index)`

- **enable_fx** / **disable_fx**
  - Enable/disable FX (bypass)
  - Required for: "disable all reverb"
  - REAPER API: `TrackFX_SetEnabled(MediaTrack *track, int fx_index, bool enabled)`

- **set_fx_param**
  - Set FX parameter value
  - Required for: "set reverb mix to 50%"
  - REAPER API: `TrackFX_SetParam(MediaTrack *track, int fx_index, int param_index, double value)`

### MIDI Operations
- **add_midi_notes** / **set_midi_notes**
  - Add MIDI notes to clip
  - Required for: "add C4 note at bar 1"
  - REAPER API: MIDI API functions (CreateMIDISource, MIDI_InsertNote, etc.)
  - Status: DSL grammar supports `.add_midi()` but no action handler

- **edit_midi_note**
  - Edit existing MIDI note (pitch, velocity, timing)
  - Required for: editing MIDI
  - REAPER API: MIDI editing functions

### Clip Operations
- **duplicate_clip** / **copy_clip**
  - Duplicate a clip
  - Required for: "copy clip to bar 5"
  - REAPER API: `CopyMediaItem(MediaItem *item)` + `PasteTrackMediaItem`

- **split_clip**
  - Split clip at time position
  - Required for: "split clip at bar 4"
  - REAPER API: `SplitMediaItem(MediaItem *item, double position)`

## Lower Priority / Advanced Actions 🟢

### Recording
- **set_track_rec_armed** / **arm_track**
  - Arm track for recording
  - REAPER API: `GetSetMediaTrackInfo` with "I_RECARM"

- **start_recording**
  - Start project recording
  - REAPER API: Transport functions

### Transport Controls
- **play** / **pause** / **stop**
  - Control playback
  - REAPER API: `OnPlayButton()`, `OnPauseButton()`, `OnStopButton()`

- **set_play_position**
  - Set play cursor position
  - REAPER API: `SetEditCurPos(double position, bool moveview, bool seekplay)`

### Time Selection
- **set_time_selection**
  - Set time selection range
  - REAPER API: `GetSet_LoopTimeRange` or similar

### Markers/Regions
- **add_marker** / **add_region**
  - Add marker or region
  - REAPER API: Marker/region functions

### Take Operations
- **add_take_fx**
  - Add FX to take (clip-level)
  - REAPER API: `TakeFX_AddByName`

- **set_take_name**
  - Name the take in a clip
  - REAPER API: Take name functions

### Envelope Operations
- **add_volume_envelope_point**
- **add_pan_envelope_point**
- **add_automation_point**
  - Add automation points
  - REAPER API: Envelope editing functions

### Track Colors
- **set_track_color**
  - Set track color for visual organization
  - REAPER API: `GetSetMediaTrackInfo` with "I_CUSTOMCOLOR"

## Notes

- Selection actions are **critical** for functional methods to work properly
- Delete actions would enable cleanup workflows
- Clip editing would enable "move clip" and "extend clip" operations
- MIDI operations are partially supported in DSL grammar but need action handlers
- FX parameter control would enable "set reverb mix to X" type commands
