#pragma once

#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// REAPER state snapshot for AI context
class MagdaState {
public:
  // Generate a JSON snapshot of current REAPER state
  // Returns JSON string (caller owns the memory)
  static char *GetStateSnapshot();

  // Get basic project info
  static void GetProjectInfo(WDL_FastString &json);

  // Get all tracks info (appends to json)
  static void GetTracksInfo(WDL_FastString &json);

  // Get play state (appends to json)
  static void GetPlayState(WDL_FastString &json);

  // Get time selection
  static void GetTimeSelection(WDL_FastString &json);

private:
  // Helper to escape JSON string
  static void EscapeJSONString(const char *str, WDL_FastString &out);
};
