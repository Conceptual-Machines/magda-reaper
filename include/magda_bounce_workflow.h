#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"
#include <functional>
#include <string>
#include <mutex>

// Forward declarations
class MediaTrack;

// Bounce mode preference - what to bounce
enum BounceMode {
  BOUNCE_MODE_FULL_TRACK = 0, // Bounce entire track
  BOUNCE_MODE_LOOP = 1,       // Bounce loop range
  BOUNCE_MODE_SELECTION = 2   // Bounce time selection
};

// Result structure for mix analysis
struct MixAnalysisResult {
  bool success = false;
  std::string responseText;  // Human-readable explanation
  std::string actionsJson;   // JSON actions to execute
};

// Result callback type for mix analysis
using MixAnalysisCallback = std::function<void(bool success, const std::string &result)>;

// Mix analysis bounce workflow
// Workflow structure:
// 1. Prepare track (copy, hide, select item) - Synchronous (safe from callback)
// 2. Render item to take - Queued, executed on main thread (outside callback)
// 3. DSP Analysis - Synchronous (in async thread)
// 4. API Call - Asynchronous (in async thread)
// 5. Delete track - Queued, executed on main thread (outside callback)
//
// Reaper commands (render, delete) are queued and processed via
// ProcessCommandQueue() which is called from the timer callback on the main
// thread.
class MagdaBounceWorkflow {
public:
  // Execute the full workflow:
  // 1. Prepare track (copy, hide, select item) - synchronous
  // 2. Queue render command (executed later on main thread)
  // 3. After render, start async thread for DSP analysis + API call
  // 4. After async completes, queue delete command
  // Returns true on success, false on error (check error_msg)
  static bool ExecuteWorkflow(BounceMode bounceMode, const char *trackType,
                              const char *userRequest,
                              WDL_FastString &error_msg);

  // Set callback for when mix analysis completes
  static void SetResultCallback(MixAnalysisCallback callback);

  // Check if there's a pending result (thread-safe)
  // Returns true if result is ready
  static bool GetPendingResult(MixAnalysisResult &result);

  // Clear pending result
  static void ClearPendingResult();

  // Get current bounce mode preference from settings
  static BounceMode GetBounceModePreference();

  // Set bounce mode preference in settings
  static void SetBounceModePreference(BounceMode mode);

  // Process queued Reaper commands (must be called from main thread, outside
  // callbacks) Returns true if any commands were processed
  static bool ProcessCommandQueue();

  // Process cleanup queue (delete tracks)
  // Returns true if any tracks were deleted
  static bool ProcessCleanupQueue();

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
