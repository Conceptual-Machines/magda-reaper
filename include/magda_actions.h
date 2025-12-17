#pragma once

#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// Action execution system for MAGDA
// Parses JSON actions and executes REAPER API calls
class MagdaActions {
public:
  // Execute actions from JSON
  // Returns true on success, false on error
  // error_msg is populated on error
  static bool ExecuteActions(const char *json, WDL_FastString &result,
                             WDL_FastString &error_msg);

private:
  // Execute a single action
  static bool ExecuteAction(const wdl_json_element *action,
                            WDL_FastString &result, WDL_FastString &error_msg);

  // Individual action handlers
  static bool CreateTrack(int index, const char *name, const char *instrument,
                          WDL_FastString &error_msg);
  static bool CreateClip(int track_index, double position, double length,
                         WDL_FastString &error_msg);
  static bool CreateClipAtBar(int track_index, int bar, int length_bars,
                              WDL_FastString &error_msg);
  static bool AddTrackFX(int track_index, const char *fxname, bool recFX,
                         WDL_FastString &error_msg);
  static bool SetTrackVolume(int track_index, double volume_db,
                             WDL_FastString &error_msg);
  static bool SetTrackPan(int track_index, double pan,
                          WDL_FastString &error_msg);
  static bool SetTrackMute(int track_index, bool mute,
                           WDL_FastString &error_msg);
  static bool SetTrackSolo(int track_index, bool solo,
                           WDL_FastString &error_msg);
  static bool SetTrackName(int track_index, const char *name,
                           WDL_FastString &error_msg);
  static bool SetTrackSelected(int track_index, bool selected,
                               WDL_FastString &error_msg);
  static bool SetClipSelected(int track_index, int clip_index, bool selected,
                              WDL_FastString &error_msg);
  static bool DeleteTrack(int track_index, WDL_FastString &error_msg);
  static bool DeleteClip(int track_index, int clip_index,
                         WDL_FastString &error_msg);
  static bool AddMIDI(int track_index, wdl_json_element *notes_array,
                      WDL_FastString &error_msg);

  // Drum pattern handler - converts grid notation to MIDI notes
  static bool AddDrumPattern(int track_index, const char *drum_name,
                             const char *grid, int velocity,
                             const char *plugin_key, WDL_FastString &error_msg);

  // Resolve drum name to MIDI note using drum mapping
  static int ResolveDrumNote(const char *drum_name, const char *plugin_key);

  // Unified property setters (Phase 1: Refactoring)
  static bool SetTrackProperties(int track_index, const char *name,
                                 const char *volume_db_str, const char *pan_str,
                                 const char *mute_str, const char *solo_str,
                                 const char *selected_str,
                                 const char *color_str,
                                 WDL_FastString &error_msg);
  static bool SetClipProperties(int track_index, const char *clip_str,
                                const char *position_str, const char *bar_str,
                                const char *name, const char *color,
                                const char *length_str,
                                const char *selected_str,
                                WDL_FastString &error_msg);

  // Automation envelope handler - supports curve-based and point-based syntax
  // curve types: fade_in, fade_out, ramp, sine, saw, square, exp_in, exp_out
  // times_in_seconds: if true, start/end are already in seconds (from bar
  // conversion)
  static bool AddAutomation(int track_index, const char *param,
                            const char *curve, double start, double end,
                            bool times_in_seconds, double from_val,
                            double to_val, double freq, double amplitude,
                            double phase, int shape,
                            wdl_json_element *points_array,
                            WDL_FastString &error_msg);

  // Helper functions
  static double BarToTime(int bar);
  static double BarsToTime(int bars);
};
