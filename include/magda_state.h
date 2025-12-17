#pragma once

#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// State filtering preferences
enum class StateFilterMode {
  All,                    // Send everything (default)
  SelectedTracksOnly,     // Only selected tracks + all their clips
  SelectedTrackClipsOnly, // Only selected tracks + only selected clips
  SelectedClipsOnly       // All tracks + only selected clips
};

struct StateFilterPreferences {
  StateFilterMode mode = StateFilterMode::All;
  int maxClipsPerTrack = -1; // -1 = unlimited
  bool includeEmptyTracks = true;

  // Metadata inclusion flags (to prevent state from getting too large)
  bool includeMidiData = false;      // Include MIDI note data (can be large)
  bool includeAudioMetadata = false; // Include audio analysis (RMS, peak, etc.)
  bool includeTakeInfo = false;      // Include take information
  bool includeFXInfo = false;        // Include FX chain info

  // Limits for large data
  int maxMidiNotesPerClip = 100; // Limit MIDI notes per clip if including MIDI
};

// REAPER state snapshot for AI context
class MagdaState {
public:
  // Generate a JSON snapshot of current REAPER state
  // Returns JSON string (caller owns the memory)
  static char *GetStateSnapshot();

  // Get basic project info
  static void GetProjectInfo(WDL_FastString &json);

  // Get all tracks info (appends to json)
  // If prefs is provided, applies filtering based on preferences
  static void GetTracksInfo(WDL_FastString &json,
                            const StateFilterPreferences *prefs = nullptr);

  // Get play state (appends to json)
  static void GetPlayState(WDL_FastString &json);

  // Get time selection
  static void GetTimeSelection(WDL_FastString &json);

  // Load state filter preferences (reads from REAPER config or uses defaults)
  static StateFilterPreferences LoadStateFilterPreferences();

private:
  // Helper to escape JSON string
  static void EscapeJSONString(const char *str, WDL_FastString &out);

  // Check if track should be included based on preferences
  static bool ShouldIncludeTrack(int trackIndex, bool isSelected, int clipCount,
                                 const StateFilterPreferences *prefs);

  // Check if clip should be included based on preferences
  static bool ShouldIncludeClip(bool trackSelected, bool clipSelected,
                                const StateFilterPreferences *prefs);
};
