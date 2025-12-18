#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// Forward declarations
class MediaTrack;

// Bounce mode preference - what to bounce
enum BounceMode {
  BOUNCE_MODE_FULL_TRACK = 0, // Bounce entire track
  BOUNCE_MODE_LOOP = 1,       // Bounce loop range
  BOUNCE_MODE_SELECTION = 2   // Bounce time selection
};

// Mix analysis bounce workflow
// Bounces selected track(s) to new track(s), hides original, runs DSP analysis,
// and sends to mix agent API
class MagdaBounceWorkflow {
public:
  // Execute the full workflow:
  // 1. Bounce selected track based on bounce mode
  // 2. Create new track with bounced audio
  // 3. Hide original track
  // 4. Run DSP analysis on bounced track
  // 5. Send analysis to mix agent API
  // Returns true on success, false on error (check error_msg)
  static bool ExecuteWorkflow(BounceMode bounceMode, const char *trackType,
                              const char *userRequest,
                              WDL_FastString &error_msg);

  // Get current bounce mode preference from settings
  static BounceMode GetBounceModePreference();

  // Set bounce mode preference in settings
  static void SetBounceModePreference(BounceMode mode);

private:
  // Step 1: Bounce track to new track
  // Returns index of new track, or -1 on error
  static int BounceTrackToNewTrack(int sourceTrackIndex, BounceMode mode,
                                   WDL_FastString &error_msg);

  // Step 2: Hide original track (set to minimal height/collapsed)
  static bool HideTrack(int trackIndex, WDL_FastString &error_msg);

  // Step 3: Run DSP analysis on bounced track
  static bool RunDSPAnalysis(int trackIndex, const char *trackName,
                             WDL_FastString &analysisJson,
                             WDL_FastString &fxJson, WDL_FastString &error_msg);

  // Step 4: Send analysis to mix agent API
  static bool SendToMixAPI(const char *analysisJson, const char *fxJson,
                           const char *trackType, const char *userRequest,
                           int trackIndex, const char *trackName,
                           WDL_FastString &responseJson,
                           WDL_FastString &error_msg);
};
