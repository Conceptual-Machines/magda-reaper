#include "magda_bounce_workflow.h"
#include "magda_api_client.h"
#include "magda_dsp_analyzer.h"
#include "magda_login_window.h"
#include "magda_settings_window.h"
#include "reaper_plugin.h"
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>

extern reaper_plugin_info_t *g_rec;

// Static HTTP client instance for API calls
static MagdaHTTPClient s_httpClient;

// Cleanup queue for tracks to delete (must be done on main thread)
static std::vector<int> s_tracksToDelete;
static std::mutex s_cleanupMutex;

// Command queue for Reaper operations that must run on main thread (outside
// callbacks)
enum ReaperCommandType { CMD_RENDER_ITEM = 0, CMD_DELETE_TRACK = 1, CMD_DELETE_TAKE = 2 };

struct ReaperCommand {
  ReaperCommandType type;
  int trackIndex;
  int itemIndex; // For render command
  bool completed;
  // For render command: start async thread after render completes
  bool startAsyncAfterRender;
  int selectedTrackIndex; // For async thread
  char trackName[256];
  char trackType[256];
  char userRequest[1024];
  // For delete take command
  void* itemPtr;  // MediaItem* to delete take from
};

static std::vector<ReaperCommand> s_reaperCommandQueue;
static std::mutex s_commandQueueMutex;

BounceMode MagdaBounceWorkflow::GetBounceModePreference() {
  // For now, default to full track
  // TODO: Load from settings/preferences
  return BOUNCE_MODE_FULL_TRACK;
}

void MagdaBounceWorkflow::SetBounceModePreference(BounceMode mode) {
  // TODO: Save to settings/preferences
  (void)mode; // Suppress unused warning
}

bool MagdaBounceWorkflow::ExecuteWorkflow(BounceMode bounceMode,
                                          const char *trackType,
                                          const char *userRequest,
                                          WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Starting mix analysis bounce workflow...\n");
  }

  // Step 0: Get selected track
  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  bool (*IsTrackSelected)(MediaTrack *) =
      (bool (*)(MediaTrack *))g_rec->GetFunc("IsTrackSelected");
  const char *(*GetSetMediaTrackInfo_String)(INT_PTR, const char *, char *,
                                             bool *) =
      (const char *(*)(INT_PTR, const char *, char *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo_String");

  // Use trackType parameter (passed from dialog)
  (void)trackType; // Will be used in SendToMixAPI

  if (!GetNumTracks || !GetTrack || !IsTrackSelected) {
    error_msg.Set("Required REAPER functions not available");
    return false;
  }

  int numTracks = GetNumTracks();
  int selectedTrackIndex = -1;
  MediaTrack *selectedTrack = nullptr;
  char trackName[256] = "Track";

  // Find selected track
  for (int i = 0; i < numTracks; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track && IsTrackSelected(track)) {
      selectedTrackIndex = i;
      selectedTrack = track;
      if (GetSetMediaTrackInfo_String) {
        char name[256];
        bool setValue = false; // false = get value
        GetSetMediaTrackInfo_String((INT_PTR)track, "P_NAME", name, &setValue);
        if (name[0]) {
          strncpy(trackName, name, sizeof(trackName) - 1);
          trackName[sizeof(trackName) - 1] = '\0';
        }
      }
      break;
    }
  }

  if (selectedTrackIndex < 0 || !selectedTrack) {
    error_msg.Set("No track selected. Please select a track first.");
    return false;
  }

  // Step 1: Handle bounce mode (set time selection if needed)
  void (*GetSet_LoopTimeRange2)(ReaProject *, bool, bool, double *, double *,
                                bool) =
      (void (*)(ReaProject *, bool, bool, double *, double *,
                bool))g_rec->GetFunc("GetSet_LoopTimeRange2");
  double (*GetProjectLength)(ReaProject *) =
      (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");

  bool needTimeSelection = false;
  double bounceStart = 0.0;
  double bounceEnd = 0.0;

  if (bounceMode == BOUNCE_MODE_LOOP || bounceMode == BOUNCE_MODE_SELECTION) {
    if (GetSet_LoopTimeRange2) {
      bool hasTimeSel = false;
      double timeSelStart = 0, timeSelEnd = 0;
      GetSet_LoopTimeRange2(nullptr, false, false, &timeSelStart, &timeSelEnd,
                            false);
      hasTimeSel = (timeSelEnd - timeSelStart) > 0.1;

      if (bounceMode == BOUNCE_MODE_SELECTION && !hasTimeSel) {
        error_msg.Set("Time selection required but none found. Please select a "
                      "time range first.");
        return false;
      }

      if (hasTimeSel) {
        bounceStart = timeSelStart;
        bounceEnd = timeSelEnd;
        needTimeSelection = true;
      }
    }

    // For loop mode, if no time selection, check loop points
    if (bounceMode == BOUNCE_MODE_LOOP && !needTimeSelection &&
        GetSet_LoopTimeRange2) {
      double loopStart = 0, loopEnd = 0;
      GetSet_LoopTimeRange2(nullptr, false, true, &loopStart, &loopEnd, false);
      if ((loopEnd - loopStart) > 0.1) {
        bounceStart = loopStart;
        bounceEnd = loopEnd;
        needTimeSelection = true;
        // Set time selection to loop range for rendering
        GetSet_LoopTimeRange2(nullptr, true, false, &bounceStart, &bounceEnd,
                              false);
      }
    }

    if (bounceMode == BOUNCE_MODE_LOOP && !needTimeSelection) {
      // No loop range, fall back to full track
      bounceMode = BOUNCE_MODE_FULL_TRACK;
    }
  }

  // PART 1: Select the item on the track for rendering (no track copy)
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  bool (*SetMediaItemSelected)(MediaItem *, bool) =
      (bool (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  int (*CountMediaItems)(ReaProject *) =
      (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");
  MediaItem *(*GetMediaItem)(ReaProject *, int) =
      (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");

  if (!CountTrackMediaItems || !GetTrackMediaItem) {
    error_msg.Set("Required REAPER functions not available");
    return false;
  }

  int itemCount = CountTrackMediaItems(selectedTrack);
  if (itemCount == 0) {
    error_msg.Set("Selected track has no media items");
    return false;
  }

  MediaItem *item = GetTrackMediaItem(selectedTrack, 0);
  if (!item) {
    error_msg.Set("Failed to get media item from track");
    return false;
  }

  // Select only this item
  if (SetMediaItemSelected && CountMediaItems && GetMediaItem) {
    int totalItems = CountMediaItems(nullptr);
    for (int i = 0; i < totalItems; i++) {
      MediaItem *otherItem = GetMediaItem(nullptr, i);
      if (otherItem) {
        SetMediaItemSelected(otherItem, false);
      }
    }
    SetMediaItemSelected(item, true);
  }

  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA: Prepared track %d for rendering (render queued)\n",
             selectedTrackIndex);
    ShowConsoleMsg(msg);
  }

  // PART 2: Queue render command (must be executed on main thread, outside
  // callback) After render completes, async thread will be started for DSP +
  // API - works on ORIGINAL track, not a copy
  {
    std::lock_guard<std::mutex> lock(s_commandQueueMutex);
    ReaperCommand cmd;
    cmd.type = CMD_RENDER_ITEM;
    cmd.trackIndex = selectedTrackIndex;  // Use original track, not a copy
    cmd.itemIndex = 0; // First item on the track
    cmd.completed = false;
    cmd.startAsyncAfterRender = true;
    cmd.selectedTrackIndex = selectedTrackIndex;
    strncpy(cmd.trackName, trackName, sizeof(cmd.trackName) - 1);
    cmd.trackName[sizeof(cmd.trackName) - 1] = '\0';
    if (trackType) {
      strncpy(cmd.trackType, trackType, sizeof(cmd.trackType) - 1);
      cmd.trackType[sizeof(cmd.trackType) - 1] = '\0';
    } else {
      cmd.trackType[0] = '\0';
    }
    if (userRequest) {
      strncpy(cmd.userRequest, userRequest, sizeof(cmd.userRequest) - 1);
      cmd.userRequest[sizeof(cmd.userRequest) - 1] = '\0';
    } else {
      cmd.userRequest[0] = '\0';
    }
    s_reaperCommandQueue.push_back(cmd);
  }

  // Return immediately - render will be executed in ProcessCommandQueue
  // which is called from timer callback on next tick
  return true;
}

int MagdaBounceWorkflow::BounceTrackToNewTrack(int sourceTrackIndex,
                                               BounceMode mode,
                                               WDL_FastString &error_msg) {
  // New approach: Copy track, hide it, render item, analyze, then delete
  // This avoids modifying the original track at all
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  void (*Main_OnCommand)(int command, int flag) =
      (void (*)(int, int))g_rec->GetFunc("Main_OnCommand");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");

  if (!GetNumTracks || !Main_OnCommand || !GetTrack) {
    error_msg.Set("Required REAPER functions not available");
    return -1;
  }

  (void)mode; // Suppress unused parameter warning

  // Store number of tracks before copy
  int tracksBefore = GetNumTracks();

  // Step 1: Select only the source track (GetTrack already declared above)
  bool (*SetTrackSelected)(MediaTrack *, bool) =
      (bool (*)(MediaTrack *, bool))g_rec->GetFunc("SetTrackSelected");

  if (!GetTrack || !SetTrackSelected) {
    error_msg.Set("GetTrack or SetTrackSelected not available");
    return -1;
  }

  // GetTrack is now declared for this function scope

  // Deselect all tracks first
  for (int i = 0; i < tracksBefore; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track) {
      SetTrackSelected(track, false);
    }
  }
  // Select only the source track
  MediaTrack *sourceTrack = GetTrack(nullptr, sourceTrackIndex);
  if (!sourceTrack) {
    error_msg.Set("Source track not found");
    return -1;
  }
  SetTrackSelected(sourceTrack, true);

  // Step 2: Copy the track (action 40062: Track: Duplicate tracks)
  Main_OnCommand(40062, 0);
  if (UpdateArrange) {
    UpdateArrange();
  }

  // Step 3: Find the new copied track (should be right after source track)
  int tracksAfter = GetNumTracks();
  if (tracksAfter <= tracksBefore) {
    error_msg.Set("Failed to copy track");
    return -1;
  }

  int copiedTrackIndex = sourceTrackIndex + 1;
  MediaTrack *copiedTrack = GetTrack(nullptr, copiedTrackIndex);
  if (!copiedTrack) {
    error_msg.Set("Failed to find copied track");
    return -1;
  }

  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA: Copied track to index %d\n",
             copiedTrackIndex);
    ShowConsoleMsg(msg);
  }

  // Step 4: Hide the copied track
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");
  if (GetSetMediaTrackInfo) {
    double minHeight = -1.0; // Collapsed
    GetSetMediaTrackInfo((INT_PTR)copiedTrack, "I_HEIGHTOVERRIDE", &minHeight,
                         nullptr);
  }

  // Step 5: Get the media item on the copied track
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");

  if (!CountTrackMediaItems || !GetTrackMediaItem) {
    error_msg.Set("CountTrackMediaItems or GetTrackMediaItem not available");
    return -1;
  }

  int itemCount = CountTrackMediaItems(copiedTrack);
  if (itemCount == 0) {
    error_msg.Set("Copied track has no media items");
    return -1;
  }

  MediaItem *copiedItem = GetTrackMediaItem(copiedTrack, 0);
  if (!copiedItem) {
    error_msg.Set("Failed to get media item from copied track");
    return -1;
  }

  // Step 6: Select only the copied item
  bool (*SetMediaItemSelected)(MediaItem *, bool) =
      (bool (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  if (SetMediaItemSelected) {
    // Deselect all items first
    int (*CountMediaItems)(ReaProject *) =
        (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");
    MediaItem *(*GetMediaItem)(ReaProject *, int) =
        (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");
    if (CountMediaItems && GetMediaItem) {
      int totalItems = CountMediaItems(nullptr);
      for (int i = 0; i < totalItems; i++) {
        MediaItem *item = GetMediaItem(nullptr, i);
        if (item) {
          SetMediaItemSelected(item, false);
        }
      }
    }
    // Select only the copied item
    SetMediaItemSelected(copiedItem, true);
  }

  // Step 7: Ensure active take is set (important for MIDI items)
  MediaItem_Take *(*GetActiveTake)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
  void (*SetActiveTake)(MediaItem_Take *) =
      (void (*)(MediaItem_Take *))g_rec->GetFunc("SetActiveTake");

  if (GetActiveTake && SetActiveTake) {
    MediaItem_Take *activeTake = GetActiveTake(copiedItem);
    if (!activeTake) {
      // No active take, try to get the first take
      int (*CountTakes)(MediaItem *) =
          (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
      MediaItem_Take *(*GetTake)(MediaItem *, int) =
          (MediaItem_Take * (*)(MediaItem *, int)) g_rec->GetFunc("GetTake");
      if (CountTakes && GetTake) {
        int takeCount = CountTakes(copiedItem);
        if (takeCount > 0) {
          MediaItem_Take *firstTake = GetTake(copiedItem, 0);
          if (firstTake) {
            SetActiveTake(firstTake);
          }
        }
      }
    }
  }

  // Step 8: Render is now queued and will be executed in ProcessCommandQueue
  // (Render must not be called from callback context)

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Prepared track for rendering (render queued)\n");
  }

  // Return the copied track index - render will be executed via queue
  // After render and analysis, we'll delete this track
  return copiedTrackIndex;
}

bool MagdaBounceWorkflow::HideTrack(int trackIndex, WDL_FastString &error_msg) {
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, trackIndex);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Set track height to minimal (0 = auto-collapsed, but we use a small value)
  // Actually, REAPER uses I_HEIGHTOVERRIDE for track height
  // Value: 0 = auto, negative = collapsed, positive = pixels
  // Use a very small negative value to collapse the track
  double minHeight = -1.0; // Collapsed
  GetSetMediaTrackInfo((INT_PTR)track, "I_HEIGHTOVERRIDE", &minHeight, nullptr);

  // Also set to minimized in TCP (Track Control Panel)
  int minimized = 1;
  GetSetMediaTrackInfo((INT_PTR)track, "I_TCPH", &minimized, nullptr);

  return true;
}

bool MagdaBounceWorkflow::RunDSPAnalysis(int trackIndex, const char *trackName,
                                         WDL_FastString &analysisJson,
                                         WDL_FastString &fxJson,
                                         WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "MAGDA: Running DSP analysis on track %d ('%s')...\n", trackIndex,
             trackName);
    ShowConsoleMsg(msg);
  }

  // Get track length for analysis
  double (*GetProjectLength)(ReaProject *) =
      (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");
  double analysisLength = 30.0; // Default 30 seconds
  if (GetProjectLength) {
    double projLen = GetProjectLength(nullptr);
    if (projLen < analysisLength) {
      analysisLength = projLen;
    }
    if (analysisLength < 1.0) {
      analysisLength = 1.0;
    }
  }

  // Configure DSP analysis
  DSPAnalysisConfig config;
  config.fftSize = 4096;
  config.analysisLength = (float)analysisLength;
  config.analyzeFullItem = true;

  // Run analysis
  DSPAnalysisResult result = MagdaDSPAnalyzer::AnalyzeTrack(trackIndex, config);

  if (!result.success) {
    error_msg.Set(result.errorMessage.Get());
    return false;
  }

  // Convert to JSON - this contains the analysis_data structure
  MagdaDSPAnalyzer::ToJSON(result, analysisJson);

  // Get FX info separately - it will be added to context
  MagdaDSPAnalyzer::GetTrackFXInfo(trackIndex, fxJson);

  return true;
}

bool MagdaBounceWorkflow::SendToMixAPI(
    const char *analysisJson, const char *fxJson, const char *trackType,
    const char *userRequest, int trackIndex, const char *trackName,
    WDL_FastString &responseJson, WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Sending analysis to mix agent API...\n");
  }

  // Set backend URL from settings
  const char *backendUrl = MagdaSettingsWindow::GetBackendURL();
  if (backendUrl && backendUrl[0]) {
    s_httpClient.SetBackendURL(backendUrl);
  }

  // Set JWT token if available
  const char *token = MagdaLoginWindow::GetStoredToken();
  if (token && token[0]) {
    s_httpClient.SetJWTToken(token);
  }

  // Build request JSON
  // Format: { "mode": "track", "analysis_data": {...}, "context": {...},
  // "user_request": "..." }
  WDL_FastString requestJson;
  requestJson.Append("{\"mode\":\"track\",");
  requestJson.Append("\"analysis_data\":");
  requestJson.Append(analysisJson);
  requestJson.Append(",\"context\":{");
  requestJson.Append("\"track_index\":");
  char trackIdxStr[32];
  snprintf(trackIdxStr, sizeof(trackIdxStr), "%d", trackIndex);
  requestJson.Append(trackIdxStr);
  requestJson.Append(",\"track_name\":\"");
  // Escape track name
  for (const char *p = trackName; *p; p++) {
    switch (*p) {
    case '"':
      requestJson.Append("\\\"");
      break;
    case '\\':
      requestJson.Append("\\\\");
      break;
    case '\n':
      requestJson.Append("\\n");
      break;
    case '\r':
      requestJson.Append("\\r");
      break;
    default:
      requestJson.Append(p, 1);
      break;
    }
  }
  requestJson.Append("\"");

  // Add track_type to context
  if (trackType && trackType[0]) {
    requestJson.Append(",\"track_type\":\"");
    // Escape track type
    for (const char *p = trackType; *p; p++) {
      switch (*p) {
      case '"':
        requestJson.Append("\\\"");
        break;
      case '\\':
        requestJson.Append("\\\\");
        break;
      case '\n':
        requestJson.Append("\\n");
        break;
      case '\r':
        requestJson.Append("\\r");
        break;
      default:
        requestJson.Append(p, 1);
        break;
      }
    }
    requestJson.Append("\"");
  }

  // Add existing_fx to context
  if (fxJson && fxJson[0]) {
    requestJson.Append(",\"existing_fx\":");
    requestJson.Append(fxJson);
  }

  requestJson.Append("}");

  // Add user_request at top level (not in context)
  if (userRequest && userRequest[0]) {
    requestJson.Append(",\"user_request\":\"");
    // Escape user request
    for (const char *p = userRequest; *p; p++) {
      switch (*p) {
      case '"':
        requestJson.Append("\\\"");
        break;
      case '\\':
        requestJson.Append("\\\\");
        break;
      case '\n':
        requestJson.Append("\\n");
        break;
      case '\r':
        requestJson.Append("\\r");
        break;
      default:
        requestJson.Append(p, 1);
        break;
      }
    }
    requestJson.Append("\"");
  }

  requestJson.Append("}");

  // Send POST request to mix analysis endpoint
  bool success = s_httpClient.SendPOSTRequest(
      "/api/v1/mix/analyze", requestJson.Get(), responseJson, error_msg,
      120); // 2 minute timeout

  if (success) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Received mix analysis response\n");
    }
  } else {
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA: Mix API error: %s\n", error_msg.Get());
      ShowConsoleMsg(msg);
    }
  }

  return success;
}

bool MagdaBounceWorkflow::ProcessCommandQueue() {
  std::lock_guard<std::mutex> lock(s_commandQueueMutex);

  if (s_reaperCommandQueue.empty()) {
    return false;
  }

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  void (*Main_OnCommand)(int command, int flag) =
      (void (*)(int, int))g_rec->GetFunc("Main_OnCommand");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  bool (*SetMediaItemSelected)(MediaItem *, bool) =
      (bool (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  int (*CountMediaItems)(ReaProject *) =
      (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");
  MediaItem *(*GetMediaItem)(ReaProject *, int) =
      (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");
  MediaItem_Take *(*GetActiveTake)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
  void (*SetActiveTake)(MediaItem_Take *) =
      (void (*)(MediaItem_Take *))g_rec->GetFunc("SetActiveTake");

  bool processedAny = false;

  // Process commands one at a time (to avoid blocking)
  for (auto it = s_reaperCommandQueue.begin();
       it != s_reaperCommandQueue.end();) {
    ReaperCommand &cmd = *it;

    if (cmd.completed) {
      // Remove completed commands
      it = s_reaperCommandQueue.erase(it);
      continue;
    }

    if (cmd.type == CMD_RENDER_ITEM) {
      // Execute render command
      if (!Main_OnCommand) {
        it = s_reaperCommandQueue.erase(it);
        continue;
      }

      // Get the track and item
      MediaTrack *track =
          GetTrack ? GetTrack(nullptr, cmd.trackIndex) : nullptr;
      if (!track) {
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg), "MAGDA: Track %d not found for render\n",
                   cmd.trackIndex);
          ShowConsoleMsg(msg);
        }
        it = s_reaperCommandQueue.erase(it);
        continue;
      }

      // Get the media item
      int itemCount = CountTrackMediaItems ? CountTrackMediaItems(track) : 0;
      if (itemCount == 0) {
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg),
                   "MAGDA: Track %d has no items for render\n", cmd.trackIndex);
          ShowConsoleMsg(msg);
        }
        it = s_reaperCommandQueue.erase(it);
        continue;
      }

      MediaItem *item =
          GetTrackMediaItem ? GetTrackMediaItem(track, cmd.itemIndex) : nullptr;
      if (!item) {
        it = s_reaperCommandQueue.erase(it);
        continue;
      }

      // Select only this item
      if (SetMediaItemSelected) {
        // Deselect all items first
        if (CountMediaItems && GetMediaItem) {
          int totalItems = CountMediaItems(nullptr);
          for (int i = 0; i < totalItems; i++) {
            MediaItem *otherItem = GetMediaItem(nullptr, i);
            if (otherItem) {
              SetMediaItemSelected(otherItem, false);
            }
          }
        }
        SetMediaItemSelected(item, true);
      }

      // Ensure active take is set
      if (GetActiveTake && SetActiveTake) {
        MediaItem_Take *activeTake = GetActiveTake(item);
        if (!activeTake) {
          int (*CountTakes)(MediaItem *) =
              (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
          MediaItem_Take *(*GetTake)(MediaItem *, int) =
              (MediaItem_Take * (*)(MediaItem *, int))
                  g_rec->GetFunc("GetTake");
          if (CountTakes && GetTake) {
            int takeCount = CountTakes(item);
            if (takeCount > 0) {
              MediaItem_Take *firstTake = GetTake(item, 0);
              if (firstTake) {
                SetActiveTake(firstTake);
              }
            }
          }
        }
      }

      // Render the item (apply FX to create new take)
      // Use action 40209: "Item: Apply track FX to items as new take"
      Main_OnCommand(40209, 0);
      if (UpdateArrange) {
        UpdateArrange();
      }

      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Applied FX to item (render completed)\n");
      }

      // Mark as completed
      cmd.completed = true;
      processedAny = true;

      // If this render should start async thread, do it now
      if (cmd.startAsyncAfterRender) {
        // Start async thread for DSP analysis + API call
        // Copy parameters for thread
        int trackIndex = cmd.trackIndex;
        int selectedTrackIndex = cmd.selectedTrackIndex;
        void* itemPtr = (void*)item;  // Store item pointer for take deletion
        char trackName[256];
        char trackType[256];
        char userRequest[1024];
        strncpy(trackName, cmd.trackName, sizeof(trackName) - 1);
        trackName[sizeof(trackName) - 1] = '\0';
        strncpy(trackType, cmd.trackType, sizeof(trackType) - 1);
        trackType[sizeof(trackType) - 1] = '\0';
        strncpy(userRequest, cmd.userRequest, sizeof(userRequest) - 1);
        userRequest[sizeof(userRequest) - 1] = '\0';

        std::thread([trackIndex, selectedTrackIndex, itemPtr, trackName, trackType,
                     userRequest]() {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

          // Step 1: Run DSP analysis on the track (analyzes the active take which is the new rendered one)
          WDL_FastString analysisJson, fxJson, error_msg, responseJson;
          bool analysisSuccess = RunDSPAnalysis(
              trackIndex, trackName, analysisJson, fxJson, error_msg);

          if (!analysisSuccess) {
            if (ShowConsoleMsg) {
              char msg[512];
              snprintf(msg, sizeof(msg), "MAGDA: DSP analysis failed: %s\n",
                       error_msg.Get());
              ShowConsoleMsg(msg);
            }
          } else {
            // Step 2: Send to mix API (asynchronous)
            if (!SendToMixAPI(analysisJson.Get(), fxJson.Get(),
                              trackType[0] ? trackType : "other",
                              userRequest[0] ? userRequest : "",
                              selectedTrackIndex, trackName, responseJson,
                              error_msg)) {
              if (ShowConsoleMsg) {
                char msg[512];
                snprintf(msg, sizeof(msg), "MAGDA: Mix API call failed: %s\n",
                         error_msg.Get());
                ShowConsoleMsg(msg);
              }
            } else {
              if (ShowConsoleMsg) {
                ShowConsoleMsg(
                    "MAGDA: Mix analysis workflow completed successfully!\n");
              }
            }
          }

          // Step 3: Queue take deletion (must be done on main thread)
          // Delete the rendered take, keeping the original take
          {
            std::lock_guard<std::mutex> cmdLock(s_commandQueueMutex);
            ReaperCommand deleteCmd;
            deleteCmd.type = CMD_DELETE_TAKE;
            deleteCmd.trackIndex = trackIndex;
            deleteCmd.itemPtr = itemPtr;
            deleteCmd.completed = false;
            s_reaperCommandQueue.push_back(deleteCmd);
          }
        }).detach();
      }

      // Move to next command
      ++it;
    } else if (cmd.type == CMD_DELETE_TRACK) {
      // Execute delete command
      bool (*DeleteTrack)(MediaTrack *) =
          (bool (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");

      if (DeleteTrack) {
        MediaTrack *track =
            GetTrack ? GetTrack(nullptr, cmd.trackIndex) : nullptr;
        if (track) {
          DeleteTrack(track);
          if (UpdateArrange) {
            UpdateArrange();
          }
          if (ShowConsoleMsg) {
            char msg[256];
            snprintf(msg, sizeof(msg), "MAGDA: Deleted track %d\n",
                     cmd.trackIndex);
            ShowConsoleMsg(msg);
          }
        }
      }

      cmd.completed = true;
      processedAny = true;
      ++it;
    } else if (cmd.type == CMD_DELETE_TAKE) {
      // Delete the active take from the item (the rendered take)
      MediaItem* item = (MediaItem*)cmd.itemPtr;
      if (item) {
        int (*CountTakes)(MediaItem *) =
            (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
        MediaItem_Take *(*GetActiveTake)(MediaItem *) =
            (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
        MediaItem_Take *(*GetTake)(MediaItem *, int) =
            (MediaItem_Take * (*)(MediaItem *, int)) g_rec->GetFunc("GetTake");
        void (*SetActiveTake)(MediaItem_Take *) =
            (void (*)(MediaItem_Take *))g_rec->GetFunc("SetActiveTake");
        
        if (CountTakes && GetActiveTake && GetTake && SetActiveTake) {
          int takeCount = CountTakes(item);
          if (takeCount > 1) {
            // Get the active take (the rendered one)
            MediaItem_Take* activeTake = GetActiveTake(item);
            
            // Find original take (the first one that's not active)
            MediaItem_Take* originalTake = nullptr;
            for (int i = 0; i < takeCount; i++) {
              MediaItem_Take* take = GetTake(item, i);
              if (take != activeTake) {
                originalTake = take;
                break;
              }
            }
            
            // Switch to original take first
            if (originalTake) {
              SetActiveTake(originalTake);
            }
            
            // Delete the rendered take using action 40129: "Take: Delete active take from items"
            // First, we need to select only this item
            bool (*SetMediaItemSelected)(MediaItem *, bool) =
                (bool (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
            if (SetMediaItemSelected && CountMediaItems && GetMediaItem) {
              int totalItems = CountMediaItems(nullptr);
              for (int i = 0; i < totalItems; i++) {
                MediaItem *otherItem = GetMediaItem(nullptr, i);
                if (otherItem) {
                  SetMediaItemSelected(otherItem, false);
                }
              }
              SetMediaItemSelected(item, true);
            }
            
            // Set the rendered take as active again so we can delete it
            if (activeTake) {
              SetActiveTake(activeTake);
              // Delete active take
              Main_OnCommand(40129, 0);  // Take: Delete active take from items
              if (UpdateArrange) {
                UpdateArrange();
              }
              if (ShowConsoleMsg) {
                ShowConsoleMsg("MAGDA: Deleted rendered take\n");
              }
            }
          } else {
            if (ShowConsoleMsg) {
              ShowConsoleMsg("MAGDA: Only one take, skipping take deletion\n");
            }
          }
        }
      }

      cmd.completed = true;
      processedAny = true;
      ++it;
    } else {
      // Unknown command type, remove it
      it = s_reaperCommandQueue.erase(it);
    }
  }

  return processedAny;
}

bool MagdaBounceWorkflow::ProcessCleanupQueue() {
  std::lock_guard<std::mutex> lock(s_cleanupMutex);

  if (s_tracksToDelete.empty()) {
    return false;
  }

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  bool (*DeleteTrack)(MediaTrack *) =
      (bool (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!DeleteTrack || !GetTrack) {
    s_tracksToDelete.clear();
    return false;
  }

  // Delete tracks one at a time
  for (int trackIndex : s_tracksToDelete) {
    MediaTrack *track = GetTrack(nullptr, trackIndex);
    if (track) {
      DeleteTrack(track);
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA: Deleted track %d\n", trackIndex);
        ShowConsoleMsg(msg);
      }
    }
  }

  s_tracksToDelete.clear();

  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}
