#include "magda_bounce_workflow.h"
#include "magda_api_client.h"
#include "magda_dsp_analyzer.h"
#include "magda_login_window.h"
#include "magda_settings_window.h"
#include "reaper_plugin.h"
#include <cstring>
#include <ctime>

extern reaper_plugin_info_t *g_rec;

// Static HTTP client instance for API calls
static MagdaHTTPClient s_httpClient;

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

  // Step 2: Bounce track to new track
  int newTrackIndex =
      BounceTrackToNewTrack(selectedTrackIndex, bounceMode, error_msg);
  if (newTrackIndex < 0) {
    return false;
  }

  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA: Bounced track %d to new track %d\n",
             selectedTrackIndex, newTrackIndex);
    ShowConsoleMsg(msg);
  }

  // Step 3: Hide original track
  if (!HideTrack(selectedTrackIndex, error_msg)) {
    // Non-fatal, continue anyway
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Warning: Could not hide original track\n");
    }
  }

  // Step 4: Run DSP analysis on bounced track
  WDL_FastString analysisJson, fxJson;
  if (!RunDSPAnalysis(newTrackIndex, trackName, analysisJson, fxJson,
                      error_msg)) {
    return false;
  }

  // Step 5: Send to mix API
  WDL_FastString responseJson;
  if (!SendToMixAPI(analysisJson.Get(), fxJson.Get(),
                    userRequest ? userRequest : "", newTrackIndex, trackName,
                    responseJson, error_msg)) {
    return false;
  }

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Mix analysis workflow completed successfully!\n");
  }

  return true;
}

int MagdaBounceWorkflow::BounceTrackToNewTrack(int sourceTrackIndex,
                                               BounceMode mode,
                                               WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  double (*GetSetProjectInfo)(ReaProject *, const char *, double, bool) =
      (double (*)(ReaProject *, const char *, double, bool))g_rec->GetFunc(
          "GetSetProjectInfo");
  bool (*GetSetProjectInfo_String)(ReaProject *, const char *, char *, bool) =
      (bool (*)(ReaProject *, const char *, char *, bool))g_rec->GetFunc(
          "GetSetProjectInfo_String");
  void (*Main_OnCommand)(int command, int flag) =
      (void (*)(int, int))g_rec->GetFunc("Main_OnCommand");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetNumTracks || !GetSetProjectInfo || !GetSetProjectInfo_String ||
      !Main_OnCommand) {
    error_msg.Set("Required REAPER functions not available");
    return -1;
  }

  // Store number of tracks before bounce
  int tracksBefore = GetNumTracks();

  // Ensure only the source track is selected before bouncing
  // (REAPER's stem render action bounces all selected tracks)
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  bool (*SetTrackSelected)(MediaTrack *, bool) =
      (bool (*)(MediaTrack *, bool))g_rec->GetFunc("SetTrackSelected");

  if (GetTrack && SetTrackSelected) {
    // Deselect all tracks first
    for (int i = 0; i < tracksBefore; i++) {
      MediaTrack *track = GetTrack(nullptr, i);
      if (track) {
        SetTrackSelected(track, false);
      }
    }
    // Select only the source track
    MediaTrack *sourceTrack = GetTrack(nullptr, sourceTrackIndex);
    if (sourceTrack) {
      SetTrackSelected(sourceTrack, true);
    }
  }

  // Get source track and its media items
  MediaTrack *sourceTrack = GetTrack(nullptr, sourceTrackIndex);
  if (!sourceTrack) {
    error_msg.Set("Source track not found");
    return -1;
  }

  // Count media items on the track
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  if (!CountTrackMediaItems) {
    error_msg.Set("CountTrackMediaItems function not available");
    return -1;
  }

  int itemCount = CountTrackMediaItems(sourceTrack);
  if (itemCount == 0) {
    error_msg.Set("Track has no media items to bounce");
    return -1;
  }

  // Get first media item (for now, we'll bounce the first item)
  // TODO: Support bouncing all items or selected items
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  if (!GetTrackMediaItem) {
    error_msg.Set("GetTrackMediaItem function not available");
    return -1;
  }

  MediaItem *sourceItem = GetTrackMediaItem(sourceTrack, 0);
  if (!sourceItem) {
    error_msg.Set("Failed to get media item from track");
    return -1;
  }

  // Select only this media item
  bool (*SetMediaItemSelected)(MediaItem *, bool) =
      (bool (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  void (*Main_OnCommandEx)(int, int, ReaProject *) =
      (void (*)(int, int, ReaProject *))g_rec->GetFunc("Main_OnCommandEx");

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
    // Select only the source item
    SetMediaItemSelected(sourceItem, true);
  }

  // Use action 40108: "Item: Apply track/take FX to items as new take"
  // This applies the track's FX chain directly to the item, creating a new take
  // without going through the master track
  Main_OnCommand(40108, 0);

  // Get the newly created take from the item
  // The new take should be the last take (take with applied FX)
  int (*CountTakes)(MediaItem *) =
      (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
  MediaItem_Take *(*GetTake)(MediaItem *, int) =
      (MediaItem_Take * (*)(MediaItem *, int)) g_rec->GetFunc("GetTake");

  if (!CountTakes || !GetTake) {
    error_msg.Set("Failed to get take functions");
    return -1;
  }

  int takeCount = CountTakes(sourceItem);
  if (takeCount == 0) {
    error_msg.Set("No takes found after applying FX");
    return -1;
  }

  // Get the last take (the one with FX applied)
  MediaItem_Take *renderedTake = GetTake(sourceItem, takeCount - 1);
  if (!renderedTake) {
    error_msg.Set("Failed to get rendered take");
    return -1;
  }

  // Get the PCM source from the rendered take
  PCM_source *(*GetMediaItemTake_Source)(MediaItem_Take *) =
      (PCM_source * (*)(MediaItem_Take *))
          g_rec->GetFunc("GetMediaItemTake_Source");
  if (!GetMediaItemTake_Source) {
    error_msg.Set("GetMediaItemTake_Source function not available");
    return -1;
  }

  PCM_source *pcmSource = GetMediaItemTake_Source(renderedTake);
  if (!pcmSource) {
    error_msg.Set("Rendered take has no PCM source");
    return -1;
  }

  // Create a new track for the bounced audio
  void (*InsertTrackInProject)(ReaProject *, int, int) =
      (void (*)(ReaProject *, int, int))g_rec->GetFunc("InsertTrackInProject");
  if (!InsertTrackInProject) {
    error_msg.Set("InsertTrackInProject function not available");
    return -1;
  }

  // Insert track right after the source track
  int newTrackIndex = sourceTrackIndex + 1;
  InsertTrackInProject(nullptr, newTrackIndex, 1);

  MediaTrack *newTrack = GetTrack(nullptr, newTrackIndex);
  if (!newTrack) {
    error_msg.Set("Failed to create new track");
    return -1;
  }

  // Create a media item on the new track
  MediaItem *(*AddMediaItemToTrack)(MediaTrack *) =
      (MediaItem * (*)(MediaTrack *)) g_rec->GetFunc("AddMediaItemToTrack");
  if (!AddMediaItemToTrack) {
    error_msg.Set("AddMediaItemToTrack function not available");
    return -1;
  }

  MediaItem *newItem = AddMediaItemToTrack(newTrack);
  if (!newItem) {
    error_msg.Set("Failed to create media item on new track");
    return -1;
  }

  // Get position and length from source item
  double (*GetMediaItemInfo_Value)(MediaItem *, const char *) = (double (*)(
      MediaItem *, const char *))g_rec->GetFunc("GetMediaItemInfo_Value");
  bool (*SetMediaItemInfo_Value)(MediaItem *, const char *, double) =
      (bool (*)(MediaItem *, const char *, double))g_rec->GetFunc(
          "SetMediaItemInfo_Value");

  if (GetMediaItemInfo_Value && SetMediaItemInfo_Value) {
    double itemPos = GetMediaItemInfo_Value(sourceItem, "D_POSITION");
    double itemLen = GetMediaItemInfo_Value(sourceItem, "D_LENGTH");
    SetMediaItemInfo_Value(newItem, "D_POSITION", itemPos);
    SetMediaItemInfo_Value(newItem, "D_LENGTH", itemLen);
  }

  // Add a take to the new item
  MediaItem_Take *(*AddTakeToMediaItem)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("AddTakeToMediaItem");
  if (!AddTakeToMediaItem) {
    error_msg.Set("AddTakeToMediaItem function not available");
    return -1;
  }

  MediaItem_Take *newTake = AddTakeToMediaItem(newItem);
  if (!newTake) {
    error_msg.Set("Failed to create take on new item");
    return -1;
  }

  // Set the PCM source to the rendered audio
  bool (*SetMediaItemTake_Source)(MediaItem_Take *, PCM_source *) = (bool (*)(
      MediaItem_Take *, PCM_source *))g_rec->GetFunc("SetMediaItemTake_Source");
  if (!SetMediaItemTake_Source) {
    error_msg.Set("SetMediaItemTake_Source function not available");
    return -1;
  }

  // Use the PCM source directly - REAPER will handle reference counting
  if (!SetMediaItemTake_Source(newTake, pcmSource)) {
    error_msg.Set("Failed to set PCM source on new take");
    return -1;
  }

  // Update arrange view
  if (UpdateArrange) {
    UpdateArrange();
  }

  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA: Created bounced track at index %d\n",
             newTrackIndex);
    ShowConsoleMsg(msg);
  }

  return newTrackIndex;
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

bool MagdaBounceWorkflow::SendToMixAPI(const char *analysisJson,
                                       const char *fxJson,
                                       const char *userRequest, int trackIndex,
                                       const char *trackName,
                                       WDL_FastString &responseJson,
                                       WDL_FastString &error_msg) {
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
      "/api/v1/magda/mix/analyze", requestJson.Get(), responseJson, error_msg,
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
