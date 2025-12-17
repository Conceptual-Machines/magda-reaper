# MAGDA Basic Features Implementation Plan

## Overview

This plan focuses on implementing all **basic/essential** features before moving to advanced functionality. Basic features are defined as:
- Core operations users expect to be available
- Features required for functional methods to work properly
- Actions that enable common workflows

## Priority Classification

### 🔴 Critical (Must Have)
Features that block basic functionality or are essential for workflows.

### 🟡 High Priority (Should Have)
Features that enable common use cases and improve user experience.

### 🟢 Medium Priority (Nice to Have)
Features that are useful but not blocking.

## Basic Features Implementation Plan

## Phase 1: Selection Actions (Critical for Functional Methods) 🔴

**Status**: ✅ **COMPLETED** (Working consistently)
**Blocking**: Functional methods (`filter`, `map`, `for_each`) cannot work without selection

### 1.1 Select/Deselect Tracks
- **Action**: `set_track_selected` / `select_track`
- **REAPER API**: `SetTrackSelected(MediaTrack *track, bool selected)`
- **Use Cases**:
  - "Select all tracks named X"
  - "Select track 1 and track 3"
  - Functional operations: `filter(tracks, track.name == "FX")`

**Implementation Tasks**:
- [x] Add `SetTrackSelected()` method to `MagdaActions`
- [x] Add action handler in `ExecuteAction()`
- [x] Update action schema in DSL parser
- [x] Add to prompt documentation
- [x] Test with functional methods

**Files to Modify**:
- `magda-reaper/include/magda_actions.h`
- `magda-reaper/src/magda_actions.cpp`
- `magda-agents-go/agents/daw/dsl_parser*.go`
- `aideas-api/internal/prompt/magda.go`

---

### 1.2 Select/Deselect Clips
- **Action**: `set_clip_selected` / `select_clip`
- **REAPER API**: `SetMediaItemSelected(MediaItem *item, bool selected)`
- **Use Cases**:
  - "Select all clips on track 1"
  - "Select clips longer than 4 bars"

**Implementation Tasks**:
- [x] Add `SetClipSelected()` method to `MagdaActions`
- [x] Add action handler (needs track + clip index or position)
- [x] Update action schema
- [x] Test selection workflows

**Files to Modify**:
- `magda-reaper/include/magda_actions.h`
- `magda-reaper/src/magda_actions.cpp`

---

## Phase 2: Deletion Actions (High Priority) 🟡

**Status**: ✅ **COMPLETED**
**Blocking**: Cleanup workflows, project management

### 2.1 Delete Track ✅
- **Action**: `delete_track` / `remove_track`
- **REAPER API**: `DeleteTrack(MediaTrack *track)`
- **Use Cases**:
  - "Delete track named X"
  - "Delete all empty tracks"
  - Cleanup workflows

**Implementation Tasks**:
- [x] Add `DeleteTrack()` method
- [x] Add action handler
- [x] Handle cleanup (confirmation optional for now)
- [x] Update documentation

### 2.2 Delete Clip ✅
- **Action**: `delete_clip` / `remove_clip`
- **REAPER API**: `DeleteTrackMediaItem(MediaTrack *track, MediaItem *item)`
- **Use Cases**:
  - "Remove all clips from track 1"
  - "Delete empty clips"
- **Features**:
  - Supports clip index, position (seconds), or bar number
  - Similar to `set_clip_selected` for flexible clip identification

**Implementation Tasks**:
- [x] Add `DeleteClip()` method
- [x] Add action handler with position/bar support
- [x] Test deletion

---

## Phase 3: Clip Editing Actions (High Priority) 🟡

**Status**: Not implemented
**Blocking**: Common editing workflows

### 3.1 Set Clip Name
- **Action**: `set_clip_name`
- **REAPER API**: `GetSetMediaItemInfo_String(MediaItem *item, "P_NAME", ...)`
- **Use Cases**:
  - "Name clip 1 as 'Intro'"
  - Organize project

**Implementation Tasks**:
- [ ] Add `SetClipName()` method
- [ ] Add action handler
- [ ] Test naming

### 3.2 Move Clip
- **Action**: `set_clip_position` / `move_clip`
- **REAPER API**: `SetMediaItemPosition(MediaItem *item, double position, bool adjust_length)`
- **Use Cases**:
  - "Move clip to bar 5"
  - "Shift clip by 2 bars"

**Implementation Tasks**:
- [ ] Add `MoveClip()` method
- [ ] Add action handler
- [ ] Support absolute and relative positioning

### 3.3 Set Clip Length
- **Action**: `set_clip_length`
- **REAPER API**: `SetMediaItemLength(MediaItem *item, double length, bool adjust_take_length)`
- **Use Cases**:
  - "Extend clip to 8 bars"
  - "Trim clip to 2 bars"

**Implementation Tasks**:
- [ ] Add `SetClipLength()` method
- [ ] Add action handler
- [ ] Test length changes

---

## Phase 4: MIDI Actions (High Priority) 🟡

**Status**: Partially implemented (DSL grammar exists, no handler)
**Blocking**: Musical content creation

### 4.1 Add MIDI Notes
- **Action**: `add_midi_notes`
- **REAPER API**: `MIDI_InsertNote(MediaItem_Take *take, ...)`
- **Use Cases**:
  - "Add C4 note at bar 1"
  - Chord progressions (after Arranger Agent integration)

**Implementation Tasks**:
- [ ] Create MIDI take if clip doesn't have one
- [ ] Implement `AddMIDINotes()` method
- [ ] Parse NoteEvent array from action
- [ ] Convert beats to PPQ (ticks)
- [ ] Insert MIDI notes
- [ ] Test with single notes and chords

**Files to Modify**:
- `magda-reaper/include/magda_actions.h`
- `magda-reaper/src/magda_actions.cpp`
- Update DSL parser to handle notes array

**Note**: This requires coordination with Arranger Agent for chord progressions (future work).

---

## Phase 5: Track Reordering (Medium Priority) 🟢

**Status**: Not implemented
**Blocking**: Organization workflows

### 5.1 Move/Reorder Track
- **Action**: `move_track` / `reorder_track`
- **REAPER API**: `ReorderSelectedTracks()` or GUID manipulation
- **Use Cases**:
  - "Move track 3 to position 1"
  - Organize tracks

**Implementation Tasks**:
- [ ] Research REAPER API for track reordering
- [ ] Add `MoveTrack()` method
- [ ] Add action handler
- [ ] Test reordering

---

## Implementation Checklist

### Phase 1: Selection (Critical) ✅
- [x] **1.1** Implement `set_track_selected` action
- [x] **1.2** Implement `set_clip_selected` action
- [x] **1.3** Test with functional methods (`filter`, `map`)
- [x] **1.4** Update integration test for selection

### Phase 2: Deletion (High Priority) ✅
- [x] **2.1** Implement `delete_track` action
- [x] **2.2** Implement `delete_clip` action
- [x] **2.3** Add safety checks (optional confirmation)
- [ ] **2.4** Test deletion workflows

### Phase 3: Clip Editing (High Priority)
- [ ] **3.1** Implement `set_clip_name` action
- [ ] **3.2** Implement `move_clip` / `set_clip_position` action
- [ ] **3.3** Implement `set_clip_length` action
- [ ] **3.4** Test clip editing workflows

### Phase 4: MIDI (High Priority)
- [ ] **4.1** Research MIDI take creation/access
- [ ] **4.2** Implement `add_midi_notes` action handler
- [ ] **4.3** Parse NoteEvent array from JSON
- [ ] **4.4** Convert timing (beats → PPQ ticks)
- [ ] **4.5** Insert MIDI notes via REAPER API
- [ ] **4.6** Test with single notes
- [ ] **4.7** Test with chord progressions (manual)

### Phase 5: Track Reordering (Medium Priority)
- [ ] **5.1** Research track reordering API
- [ ] **5.2** Implement `move_track` action
- [ ] **5.3** Test reordering

## Testing Strategy

### Unit Tests (REAPER Extension)
- [ ] Test each action handler independently
- [ ] Test error cases (invalid track index, etc.)
- [ ] Test edge cases (empty projects, etc.)

### Integration Tests (API)
- [ ] Test complete workflows (create → edit → delete)
- [ ] Test functional methods with selection
- [ ] Test MIDI note insertion

### Manual Testing Checklist
- [ ] Create track → select → rename → delete
- [ ] Create clip → move → rename → delete
- [ ] Add MIDI notes to clip
- [ ] Use filter to select tracks by name
- [ ] Use map to rename multiple tracks

## Documentation Updates

For each implemented action:
- [ ] Add to action schema
- [ ] Update DSL grammar (if needed)
- [ ] Add to prompt documentation
- [ ] Add example use cases
- [ ] Update MISSING_ACTIONS.md

## Estimated Effort

### Phase 1: Selection (2-3 days)
- Track selection: 4-6 hours
- Clip selection: 4-6 hours
- Testing: 2-4 hours

### Phase 2: Deletion (2 days)
- Track deletion: 3-4 hours
- Clip deletion: 3-4 hours
- Testing: 2-3 hours

### Phase 3: Clip Editing (3 days)
- Clip name: 2-3 hours
- Clip position: 4-6 hours
- Clip length: 4-6 hours
- Testing: 3-4 hours

### Phase 4: MIDI (4-5 days)
- Research & design: 4-6 hours
- Implementation: 8-12 hours
- Testing: 4-6 hours

### Phase 5: Track Reordering (2 days)
- Research API: 3-4 hours
- Implementation: 4-6 hours
- Testing: 2-3 hours

**Total Estimated Time**: 13-17 days

## Order of Implementation

**Recommended order** (by blocking dependencies):

1. **Selection Actions** (Phase 1) - Critical for functional methods
2. **MIDI Actions** (Phase 4) - High value, partially started
3. **Clip Editing** (Phase 3) - Common workflows
4. **Deletion Actions** (Phase 2) - Cleanup, can come after creation
5. **Track Reordering** (Phase 5) - Nice to have, less critical

## Next Steps

1. ✅ Review and approve this plan
2. ⏳ Start with Phase 1 (Selection Actions)
3. ⏳ Implement one action at a time
4. ⏳ Test thoroughly before moving to next
5. ⏳ Update documentation as we go

## Notes

- **Avoid scope creep**: Stick to basic features only
- **Test incrementally**: Test each action before moving to next
- **Document as we go**: Update docs immediately after implementation
- **Integration tests**: Add tests for functional methods once selection works
