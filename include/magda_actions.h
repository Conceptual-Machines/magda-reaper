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

  // Helper functions
  static double BarToTime(int bar);
  static double BarsToTime(int bars);
};
