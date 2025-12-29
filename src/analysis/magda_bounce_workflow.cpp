#include "magda_bounce_workflow.h"
#include "../WDL/WDL/jsonparse.h"
#include "../api/magda_openai.h"
#include "magda_api_client.h"
#include "magda_dsp_analyzer.h"
#include "magda_imgui_login.h"
#include "reaper_plugin.h"
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

extern reaper_plugin_info_t *g_rec;

// Static HTTP client instance for API calls
static MagdaHTTPClient s_httpClient;

// Cleanup queue for tracks to delete (must be done on main thread)
static std::vector<int> s_tracksToDelete;
static std::mutex s_cleanupMutex;

// Command queue for Reaper operations that must run on main thread (outside
// callbacks)
enum ReaperCommandType {
  CMD_RENDER_ITEM = 0,
  CMD_DELETE_TRACK = 1,
  CMD_DELETE_TAKE = 2,
  CMD_DSP_ANALYZE = 3,
  CMD_MULTI_TRACK_COMPARE = 4
};

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
  void *itemPtr; // MediaItem* to delete take from
  int takeIndex; // Index of take to delete
  // For deferred commands - wait until file is ready
  int deferCount;    // Max attempts before giving up
  long lastFileSize; // Last checked file size (for stability check)
  int stableCount;   // How many ticks file size has been stable
};

static std::vector<ReaperCommand> s_reaperCommandQueue;
static std::mutex s_commandQueueMutex;

// Result storage for async mix analysis
static std::mutex s_resultMutex;
static bool s_resultPending = false;
static MixAnalysisResult s_result;
static MixAnalysisCallback s_resultCallback = nullptr;

void MagdaBounceWorkflow::SetResultCallback(MixAnalysisCallback callback) {
  std::lock_guard<std::mutex> lock(s_resultMutex);
  s_resultCallback = callback;
}

bool MagdaBounceWorkflow::GetPendingResult(MixAnalysisResult &result) {
  std::lock_guard<std::mutex> lock(s_resultMutex);
  if (!s_resultPending) {
    return false;
  }
  result = s_result;
  return true;
}

void MagdaBounceWorkflow::ClearPendingResult() {
  std::lock_guard<std::mutex> lock(s_resultMutex);
  s_resultPending = false;
  s_result = MixAnalysisResult();
}

// Streaming state for real-time text streaming
static std::mutex s_streamMutex;
static MixStreamingState s_streamState;

bool MagdaBounceWorkflow::GetStreamingState(MixStreamingState &state) {
  std::lock_guard<std::mutex> lock(s_streamMutex);
  if (!s_streamState.isStreaming && !s_streamState.streamComplete) {
    return false; // No streaming active or complete
  }
  state = s_streamState;
  return true;
}

void MagdaBounceWorkflow::AppendStreamText(const std::string &text) {
  std::lock_guard<std::mutex> lock(s_streamMutex);
  s_streamState.streamBuffer += text;
}

void MagdaBounceWorkflow::StartStreaming() {
  std::lock_guard<std::mutex> lock(s_streamMutex);
  s_streamState.isStreaming = true;
  s_streamState.streamComplete = false;
  s_streamState.streamError = false;
  s_streamState.streamBuffer.clear();
  s_streamState.errorMessage.clear();
}

void MagdaBounceWorkflow::CompleteStreaming(bool success,
                                            const std::string &error) {
  std::lock_guard<std::mutex> lock(s_streamMutex);
  s_streamState.isStreaming = false;
  s_streamState.streamComplete = true;
  s_streamState.streamError = !success;
  s_streamState.errorMessage = error;
}

void MagdaBounceWorkflow::ClearStreamingState() {
  std::lock_guard<std::mutex> lock(s_streamMutex);
  s_streamState = MixStreamingState();
}

// Phase tracking for UI status display
static std::mutex s_phaseMutex;
static MixAnalysisPhase s_currentPhase = MIX_PHASE_IDLE;

MixAnalysisPhase MagdaBounceWorkflow::GetCurrentPhase() {
  std::lock_guard<std::mutex> lock(s_phaseMutex);
  return s_currentPhase;
}

void MagdaBounceWorkflow::SetCurrentPhase(MixAnalysisPhase phase) {
  std::lock_guard<std::mutex> lock(s_phaseMutex);
  s_currentPhase = phase;
}

// Helper to store result (called from background thread)
static void StoreResult(bool success, const std::string &responseText,
                        const std::string &actionsJson = "") {
  // Set phase to idle (workflow complete)
  MagdaBounceWorkflow::SetCurrentPhase(MIX_PHASE_IDLE);

  std::lock_guard<std::mutex> lock(s_resultMutex);
  s_resultPending = true;
  s_result.success = success;
  s_result.responseText = responseText;
  s_result.actionsJson = actionsJson;

  // Call callback if set
  if (s_resultCallback) {
    s_resultCallback(success, responseText);
  }
}

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

  // Set phase to rendering
  SetCurrentPhase(MIX_PHASE_RENDERING);

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
        // !!! CRITICAL: DO NOT REMOVE THE = {0} INITIALIZATION !!!
        // Without it, nameless tracks cause garbage filenames like "Pò_÷"
        // which break REAPER's render. This took hours to debug.
        char name[256] = {0};
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
    snprintf(msg, sizeof(msg),
             "MAGDA: Prepared track %d for rendering (render queued)\n",
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
    cmd.trackIndex = selectedTrackIndex; // Use original track, not a copy
    cmd.itemIndex = 0;                   // First item on the track
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

bool MagdaBounceWorkflow::ExecuteMasterWorkflow(const char *userRequest,
                                                WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Starting master analysis workflow...\n");
  }

  // For master analysis, we need to:
  // 1. Get the project time range
  // 2. Create a temporary track
  // 3. Add a receive from master
  // 4. Render time selection to new take
  // 5. Analyze the rendered audio
  // 6. Clean up

  // Get required functions
  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  MediaTrack *(*GetMasterTrack)(ReaProject *) =
      (MediaTrack * (*)(ReaProject *)) g_rec->GetFunc("GetMasterTrack");
  void (*InsertTrackInProject)(ReaProject *, int, int) =
      (void (*)(ReaProject *, int, int))g_rec->GetFunc("InsertTrackInProject");
  bool (*SetTrackSelected)(MediaTrack *, bool) =
      (bool (*)(MediaTrack *, bool))g_rec->GetFunc("SetTrackSelected");
  double (*GetProjectLength)(ReaProject *) =
      (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");
  void (*GetSet_LoopTimeRange2)(ReaProject *, bool, bool, double *, double *,
                                bool) =
      (void (*)(ReaProject *, bool, bool, double *, double *,
                bool))g_rec->GetFunc("GetSet_LoopTimeRange2");
  int (*CreateTrackSend)(MediaTrack *, MediaTrack *) =
      (int (*)(MediaTrack *, MediaTrack *))g_rec->GetFunc("CreateTrackSend");
  bool (*SetTrackSendInfo_Value)(MediaTrack *, int, int, const char *, double) =
      (bool (*)(MediaTrack *, int, int, const char *, double))g_rec->GetFunc(
          "SetTrackSendInfo_Value");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");
  const char *(*GetSetMediaTrackInfo_String)(INT_PTR, const char *, char *,
                                             bool *) =
      (const char *(*)(INT_PTR, const char *, char *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo_String");
  void (*Main_OnCommand)(int command, int flag) =
      (void (*)(int, int))g_rec->GetFunc("Main_OnCommand");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetNumTracks || !GetTrack || !GetMasterTrack || !InsertTrackInProject ||
      !SetTrackSelected || !GetProjectLength || !GetSet_LoopTimeRange2) {
    error_msg.Set("Required REAPER functions not available");
    return false;
  }

  // Step 1: Get project length and set time selection
  double projectLength = GetProjectLength(nullptr);
  if (projectLength < 0.1) {
    error_msg.Set("Project is empty or too short");
    return false;
  }

  // Save current time selection
  double savedTimeSelStart = 0, savedTimeSelEnd = 0;
  GetSet_LoopTimeRange2(nullptr, false, false, &savedTimeSelStart,
                        &savedTimeSelEnd, false);

  // Set time selection to full project
  double timeSelStart = 0.0;
  double timeSelEnd = projectLength;
  GetSet_LoopTimeRange2(nullptr, true, false, &timeSelStart, &timeSelEnd,
                        false);

  // Step 2: Create a new track at the end
  int numTracks = GetNumTracks();
  int newTrackIndex = numTracks;
  InsertTrackInProject(nullptr, newTrackIndex, 1);

  MediaTrack *newTrack = GetTrack(nullptr, newTrackIndex);
  if (!newTrack) {
    // Restore time selection
    GetSet_LoopTimeRange2(nullptr, true, false, &savedTimeSelStart,
                          &savedTimeSelEnd, false);
    error_msg.Set("Failed to create temporary track");
    return false;
  }

  // Name the track
  if (GetSetMediaTrackInfo_String) {
    bool setValue = true;
    char trackName[] = "MAGDA_MASTER_ANALYSIS";
    GetSetMediaTrackInfo_String((INT_PTR)newTrack, "P_NAME", trackName,
                                &setValue);
  }

  // Step 3: Create receive from master track
  MediaTrack *masterTrack = GetMasterTrack(nullptr);
  if (!masterTrack) {
    // Delete the temp track
    bool (*DeleteTrack)(MediaTrack *) =
        (bool (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
    if (DeleteTrack) {
      DeleteTrack(newTrack);
    }
    // Restore time selection
    GetSet_LoopTimeRange2(nullptr, true, false, &savedTimeSelStart,
                          &savedTimeSelEnd, false);
    error_msg.Set("Failed to get master track");
    return false;
  }

  // Create send from master to our new track
  // Note: In REAPER, sends FROM master are actually "hardware output" style
  // Instead, we'll use a different approach: record the master output directly
  // by using the "Render selected area of tracks to stereo post-fader stem
  // tracks" action But first, let's try creating a send from master

  if (CreateTrackSend && SetTrackSendInfo_Value) {
    // Create a send from the master to our temp track
    // Actually, REAPER doesn't allow sends FROM master in the normal way
    // We need to use a different approach
  }

  // Alternative approach: Select the master track and use the stem render
  // action to create an item on a new track

  // For now, let's use a simpler approach:
  // 1. Deselect all tracks
  // 2. Select master track
  // 3. Use "Track: Render selected area of tracks to stereo post-fader stem
  // tracks (and mute originals)"
  //    Action ID: 41719

  // Deselect all tracks
  for (int i = 0; i < numTracks; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track) {
      SetTrackSelected(track, false);
    }
  }

  // Delete the temp track we created (we'll use the one created by the render
  // action)
  bool (*DeleteTrack)(MediaTrack *) =
      (bool (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
  if (DeleteTrack && newTrack) {
    DeleteTrack(newTrack);
  }

  // Select master track for stem rendering
  // Note: Master track selection works differently in REAPER
  // Instead, let's use the "Render project to disk" approach with specific
  // settings

  // Actually, the easiest approach for master analysis is to render the entire
  // project to a temp file and analyze that. But that requires file I/O which
  // is more complex.

  // For MVP, let's use a different approach:
  // Select ALL tracks and use "Render tracks to mono/stereo stem tracks" This
  // gives us the full mix as a rendered item

  // Select all tracks
  for (int i = 0; i < numTracks; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track) {
      SetTrackSelected(track, true);
    }
  }

  // Update the track count since we deleted one
  numTracks = GetNumTracks();

  if (ShowConsoleMsg) {
    ShowConsoleMsg(
        "MAGDA: Rendering master output (stem render of all tracks)...\n");
  }

  // Use action: "Track: Render selected area of tracks to stereo post-fader
  // stem tracks"
  // This renders all selected tracks post-fader (including master bus
  // processing) to new tracks Action ID: 41716 = "Render tracks to stereo
  // post-fader stem tracks"
  if (Main_OnCommand) {
    Main_OnCommand(41716, 0); // Render to stereo stem tracks (post-fader)
  }

  if (UpdateArrange) {
    UpdateArrange();
  }

  // Find the newly created stem track(s) - should be after our original tracks
  int newNumTracks = GetNumTracks();
  int stemTrackIndex = -1;

  // The stem tracks are created at the end
  if (newNumTracks > numTracks) {
    // Take the last created track as our master stem
    stemTrackIndex = newNumTracks - 1;

    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: Created master stem at track index %d\n",
               stemTrackIndex);
      ShowConsoleMsg(msg);
    }
  } else {
    // Restore time selection
    GetSet_LoopTimeRange2(nullptr, true, false, &savedTimeSelStart,
                          &savedTimeSelEnd, false);
    error_msg.Set("Failed to create stem render - no new tracks created");
    return false;
  }

  // Restore time selection
  GetSet_LoopTimeRange2(nullptr, true, false, &savedTimeSelStart,
                        &savedTimeSelEnd, false);

  // Now we have a rendered stem track - queue it for analysis
  // Get track name for the API
  MediaTrack *stemTrack = GetTrack(nullptr, stemTrackIndex);
  char trackName[256] = "Master";

  // Get the item on the stem track
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");

  if (!CountTrackMediaItems || !GetTrackMediaItem || !stemTrack) {
    error_msg.Set("Failed to access stem track");
    return false;
  }

  int itemCount = CountTrackMediaItems(stemTrack);
  if (itemCount == 0) {
    error_msg.Set("Stem track has no media items");
    return false;
  }

  MediaItem *stemItem = GetTrackMediaItem(stemTrack, 0);
  if (!stemItem) {
    error_msg.Set("Failed to get item from stem track");
    return false;
  }

  // Queue the DSP analysis command
  {
    std::lock_guard<std::mutex> lock(s_commandQueueMutex);
    ReaperCommand cmd;
    cmd.type = CMD_DSP_ANALYZE;
    cmd.trackIndex = stemTrackIndex;
    cmd.itemIndex = 0;
    cmd.completed = false;
    cmd.startAsyncAfterRender = true;
    cmd.selectedTrackIndex = stemTrackIndex;
    cmd.itemPtr = (void *)stemItem;
    cmd.takeIndex = 0;
    cmd.deferCount = 50; // Wait for file to stabilize
    cmd.lastFileSize = 0;
    cmd.stableCount = 0;
    strncpy(cmd.trackName, trackName, sizeof(cmd.trackName) - 1);
    cmd.trackName[sizeof(cmd.trackName) - 1] = '\0';
    strncpy(cmd.trackType, "master", sizeof(cmd.trackType) - 1);
    cmd.trackType[sizeof(cmd.trackType) - 1] = '\0';
    if (userRequest) {
      strncpy(cmd.userRequest, userRequest, sizeof(cmd.userRequest) - 1);
      cmd.userRequest[sizeof(cmd.userRequest) - 1] = '\0';
    } else {
      cmd.userRequest[0] = '\0';
    }
    s_reaperCommandQueue.push_back(cmd);
  }

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Master analysis queued for processing\n");
  }

  return true;
}

bool MagdaBounceWorkflow::ExecuteMultiTrackWorkflow(const char *compareArgs,
                                                    WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Starting multi-track comparison workflow...\n");
  }

  if (!compareArgs || !compareArgs[0]) {
    error_msg.Set("No comparison arguments provided");
    return false;
  }

  // Get required functions
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

  // Parse track identifiers from compareArgs
  // Format examples:
  // - "selected" - compare all selected tracks
  // - "track 1 and track 2" - compare tracks by index
  // - "drums bass" - compare tracks by name (partial match)
  std::vector<int> trackIndices;

  std::string args(compareArgs);
  std::transform(args.begin(), args.end(), args.begin(), ::tolower);

  // Check for "selected" keyword
  if (args.find("selected") != std::string::npos) {
    // Get all selected tracks
    int numTracks = GetNumTracks();
    for (int i = 0; i < numTracks; i++) {
      MediaTrack *track = GetTrack(nullptr, i);
      if (track && IsTrackSelected(track)) {
        trackIndices.push_back(i);
      }
    }

    if (trackIndices.empty()) {
      error_msg.Set(
          "No tracks selected. Please select at least two tracks to compare.");
      return false;
    }

    if (trackIndices.size() < 2) {
      error_msg.Set("Please select at least two tracks to compare.");
      return false;
    }
  } else {
    // Parse track names or indices from args
    // Try to match track names first
    std::vector<std::string> trackIdentifiers;
    std::string current;
    for (char c : args) {
      if (c == ' ' || c == ',' || c == '&' ||
          (args.find(" and ") != std::string::npos &&
           current.find("and") != std::string::npos)) {
        if (!current.empty()) {
          // Remove "and" keyword
          if (current != "and") {
            trackIdentifiers.push_back(current);
          }
          current.clear();
        }
      } else {
        current += c;
      }
    }
    if (!current.empty() && current != "and") {
      trackIdentifiers.push_back(current);
    }

    // Also try to extract track numbers (e.g., "track 1", "1", etc.)
    for (const std::string &ident : trackIdentifiers) {
      // Try to find track by number
      char *endPtr;
      long trackNum = strtol(ident.c_str(), &endPtr, 10);
      if (*endPtr == '\0' && trackNum >= 0) {
        // It's a number - use as track index
        int idx = (int)trackNum;
        int numTracks = GetNumTracks();
        if (idx < numTracks) {
          if (std::find(trackIndices.begin(), trackIndices.end(), idx) ==
              trackIndices.end()) {
            trackIndices.push_back(idx);
          }
        }
      } else {
        // Try to find track by name (partial match)
        int numTracks = GetNumTracks();
        for (int i = 0; i < numTracks; i++) {
          MediaTrack *track = GetTrack(nullptr, i);
          if (track && GetSetMediaTrackInfo_String) {
            char trackName[256] = {0};
            bool setValue = false;
            GetSetMediaTrackInfo_String((INT_PTR)track, "P_NAME", trackName,
                                        &setValue);
            if (trackName[0]) {
              std::string name(trackName);
              std::transform(name.begin(), name.end(), name.begin(), ::tolower);
              if (name.find(ident) != std::string::npos ||
                  ident.find(name) != std::string::npos) {
                if (std::find(trackIndices.begin(), trackIndices.end(), i) ==
                    trackIndices.end()) {
                  trackIndices.push_back(i);
                  break; // Match first track with this name
                }
              }
            }
          }
        }
      }
    }

    if (trackIndices.empty()) {
      error_msg.Set("No tracks found matching the provided identifiers. Try: "
                    "'@mix:compare selected' or specify track names/indices.");
      return false;
    }

    if (trackIndices.size() < 2) {
      error_msg.Set(
          "Please specify at least two tracks to compare (e.g., '@mix:compare "
          "track1 and track2' or '@mix:compare selected').");
      return false;
    }
  }

  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA: Comparing %zu tracks...\n",
             trackIndices.size());
    ShowConsoleMsg(msg);
  }

  // For now, we'll queue each track for analysis sequentially
  // TODO: Could optimize to do parallel analysis in the future
  // Store track indices for sequential processing
  // This is a simplified implementation - in practice, you'd want to
  // queue all tracks and process them together

  // For MVP, let's analyze tracks one by one and collect results
  // We'll store the analysis results and send them all together at the end
  // This requires modifying the workflow to support collecting multiple results

  // For now, let's just analyze the first two tracks and send them together
  // as a proof of concept
  if (trackIndices.size() > 2) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Warning - comparing first 2 tracks (full "
                     "multi-track support coming soon)\n");
    }
    trackIndices.resize(2);
  }

  // For MVP, queue the first track for analysis with a note that it's a
  // comparison Future: Implement full multi-track analysis that collects all
  // results and sends together For now, we'll analyze tracks sequentially and
  // the user can compare them or we can enhance the backend to accept multiple
  // tracks in one request

  if (trackIndices.size() > 2 && ShowConsoleMsg) {
    ShowConsoleMsg(
        "MAGDA: Comparing first 2 tracks (analyzing sequentially)\n");
  }

  // Queue each track for analysis - they'll be processed sequentially
  // TODO: Enhance to collect all results and send in one multi_track API call
  for (size_t i = 0; i < trackIndices.size() && i < 2; i++) {
    int trackIdx = trackIndices[i];
    MediaTrack *track = GetTrack(nullptr, trackIdx);
    if (!track) {
      continue;
    }

    // Get track name
    char trackName[256] = "Track";
    if (GetSetMediaTrackInfo_String) {
      char name[256] = {0};
      bool setValue = false;
      GetSetMediaTrackInfo_String((INT_PTR)track, "P_NAME", name, &setValue);
      if (name[0]) {
        strncpy(trackName, name, sizeof(trackName) - 1);
        trackName[sizeof(trackName) - 1] = '\0';
      }
    }

    // Queue render command for this track
    {
      std::lock_guard<std::mutex> lock(s_commandQueueMutex);
      ReaperCommand cmd;
      cmd.type = CMD_RENDER_ITEM;
      cmd.trackIndex = trackIdx;
      cmd.itemIndex = 0;
      cmd.completed = false;
      cmd.startAsyncAfterRender = true;
      cmd.selectedTrackIndex = trackIdx;
      strncpy(cmd.trackName, trackName, sizeof(cmd.trackName) - 1);
      cmd.trackName[sizeof(cmd.trackName) - 1] = '\0';
      strncpy(cmd.trackType, i == 0 ? "compare_track1" : "compare_track2",
              sizeof(cmd.trackType) - 1);
      cmd.trackType[sizeof(cmd.trackType) - 1] = '\0';
      // Add note that this is a comparison
      std::string userReq = "Compare this track with ";
      if (trackIndices.size() > 1) {
        MediaTrack *otherTrack = GetTrack(nullptr, trackIndices[1 - i]);
        if (otherTrack && GetSetMediaTrackInfo_String) {
          char otherName[256] = {0};
          bool setValue = false;
          GetSetMediaTrackInfo_String((INT_PTR)otherTrack, "P_NAME", otherName,
                                      &setValue);
          if (otherName[0]) {
            userReq += otherName;
          } else {
            userReq += "track ";
            userReq += std::to_string(trackIndices[1 - i]);
          }
        }
      }
      strncpy(cmd.userRequest, userReq.c_str(), sizeof(cmd.userRequest) - 1);
      cmd.userRequest[sizeof(cmd.userRequest) - 1] = '\0';
      s_reaperCommandQueue.push_back(cmd);
    }

    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA: Queued track %d (%s) for comparison\n",
               trackIdx, trackName);
      ShowConsoleMsg(msg);
    }
  }

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
    ShowConsoleMsg("MAGDA: Sending analysis to OpenAI directly...\n");
  }

  // Set phase to API call
  SetCurrentPhase(MIX_PHASE_API_CALL);

  // Check if OpenAI API key is available
  MagdaOpenAI *openai = GetMagdaOpenAI();
  if (!openai || !openai->HasAPIKey()) {
    error_msg.Set(
        "OpenAI API key not configured. Please set it in MAGDA > API Keys.");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: OpenAI API key not configured\n");
    }
    return false;
  }

  // Build track context JSON
  WDL_FastString contextJson;
  contextJson.Set("{");
  contextJson.Append("\"track_index\":");
  char trackIdxStr[32];
  snprintf(trackIdxStr, sizeof(trackIdxStr), "%d", trackIndex);
  contextJson.Append(trackIdxStr);
  contextJson.Append(",\"track_name\":\"");
  // Escape track name
  for (const char *p = trackName; p && *p; p++) {
    switch (*p) {
    case '"':
      contextJson.Append("\\\"");
      break;
    case '\\':
      contextJson.Append("\\\\");
      break;
    case '\n':
      contextJson.Append("\\n");
      break;
    default:
      contextJson.Append(p, 1);
      break;
    }
  }
  contextJson.Append("\"");
  if (trackType && trackType[0]) {
    contextJson.Append(",\"track_type\":\"");
    for (const char *p = trackType; *p; p++) {
      switch (*p) {
      case '"':
        contextJson.Append("\\\"");
        break;
      case '\\':
        contextJson.Append("\\\\");
        break;
      default:
        contextJson.Append(p, 1);
        break;
      }
    }
    contextJson.Append("\"");
  }
  if (fxJson && fxJson[0]) {
    contextJson.Append(",\"existing_fx\":");
    contextJson.Append(fxJson);
  }
  contextJson.Append("}");

  // Start streaming state
  StartStreaming();

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Calling OpenAI for mix analysis...\n");
  }

  // Local accumulator to avoid race condition with UI clearing streaming state
  // The UI polls and clears the state, so we need our own copy
  std::string localAccumulator;

  // Streaming callback that accumulates text both locally and to UI state
  auto streamCallback = [&localAccumulator](const char *text,
                                            bool is_done) -> bool {
    if (text && *text) {
      localAccumulator += text; // Local accumulator (safe from UI clearing)
      MagdaBounceWorkflow::AppendStreamText(text); // For UI streaming display
    }
    if (is_done) {
      MagdaBounceWorkflow::CompleteStreaming(true);
    }
    return true; // Continue streaming
  };

  // Call OpenAI directly
  bool success = openai->GenerateMixFeedback(
      analysisJson, contextJson.Get(), userRequest, streamCallback, error_msg);

  if (!success) {
    CompleteStreaming(false, error_msg.Get());
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA: OpenAI mix analysis error: %s\n",
               error_msg.Get());
      ShowConsoleMsg(msg);
    }
    return false;
  }

  // Use local accumulator (avoids race with UI clearing streaming state)
  if (!localAccumulator.empty()) {
    responseJson.Set(localAccumulator.c_str());
    // Also store result for chat polling
    StoreResult(true, localAccumulator);
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA: Mix analysis completed (%zu chars)\n",
               localAccumulator.size());
      ShowConsoleMsg(msg);
    }
    return true;
  }

  error_msg.Set("No response received from OpenAI");
  return false;
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

  // Collect new commands to add after the loop (to avoid iterator invalidation)
  std::vector<ReaperCommand> commandsToAdd;

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

      // Count takes BEFORE render so we know which one is new
      int (*CountTakesFunc)(MediaItem *) =
          (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
      int takesBefore = CountTakesFunc ? CountTakesFunc(item) : 0;

      // Ensure take has a valid name before rendering (prevents garbage
      // filenames)
      if (GetActiveTake) {
        MediaItem_Take *activeTake = GetActiveTake(item);
        if (activeTake) {
          bool (*GetSetMediaItemTakeInfo_String)(MediaItem_Take *, const char *,
                                                 char *, bool) =
              (bool (*)(MediaItem_Take *, const char *, char *,
                        bool))g_rec->GetFunc("GetSetMediaItemTakeInfo_String");

          if (GetSetMediaItemTakeInfo_String) {
            // Check if take has a name
            char takeName[512] = {0};
            GetSetMediaItemTakeInfo_String(activeTake, "P_NAME", takeName,
                                           false);

            // If no name, set a default based on track name or generic name
            if (takeName[0] == '\0') {
              char defaultName[256];
              if (cmd.trackName[0] != '\0') {
                snprintf(defaultName, sizeof(defaultName), "%s", cmd.trackName);
              } else {
                snprintf(defaultName, sizeof(defaultName), "Track_%d",
                         cmd.trackIndex + 1);
              }
              GetSetMediaItemTakeInfo_String(activeTake, "P_NAME", defaultName,
                                             true);
              if (ShowConsoleMsg) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "MAGDA: Set default take name: '%s'\n", defaultName);
                ShowConsoleMsg(msg);
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
        char msg[256];
        int takesAfter = CountTakesFunc ? CountTakesFunc(item) : 0;
        snprintf(msg, sizeof(msg),
                 "MAGDA: Applied FX to item (takes: %d -> %d)\n", takesBefore,
                 takesAfter);
        ShowConsoleMsg(msg);
      }

      // Mark as completed
      cmd.completed = true;
      processedAny = true;

      // If this render should continue with DSP analysis, queue it
      // DSP must run on main thread because audio accessor API is not
      // thread-safe
      if (cmd.startAsyncAfterRender) {
        // Queue DSP analysis command (runs on main thread)
        // Note: Active take will be set in CMD_DSP_ANALYZE right before
        // analysis
        ReaperCommand dspCmd;
        dspCmd.type = CMD_DSP_ANALYZE;
        dspCmd.trackIndex = cmd.trackIndex;
        dspCmd.selectedTrackIndex = cmd.selectedTrackIndex;
        dspCmd.itemPtr = (void *)item;
        dspCmd.takeIndex = takesBefore;
        dspCmd.completed = false;
        dspCmd.deferCount = 100; // Max 100 attempts (~3-5 seconds)
        dspCmd.lastFileSize = 0; // Will check file size stability
        dspCmd.stableCount = 0;  // Need 3 stable readings
        strncpy(dspCmd.trackName, cmd.trackName, sizeof(dspCmd.trackName) - 1);
        dspCmd.trackName[sizeof(dspCmd.trackName) - 1] = '\0';
        strncpy(dspCmd.trackType, cmd.trackType, sizeof(dspCmd.trackType) - 1);
        dspCmd.trackType[sizeof(dspCmd.trackType) - 1] = '\0';
        strncpy(dspCmd.userRequest, cmd.userRequest,
                sizeof(dspCmd.userRequest) - 1);
        dspCmd.userRequest[sizeof(dspCmd.userRequest) - 1] = '\0';
        commandsToAdd.push_back(dspCmd);
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
      // Delete the rendered take from the item by index
      MediaItem *item = (MediaItem *)cmd.itemPtr;
      if (item) {
        int (*CountTakes)(MediaItem *) =
            (int (*)(MediaItem *))g_rec->GetFunc("CountTakes");
        MediaItem_Take *(*GetTake)(MediaItem *, int) =
            (MediaItem_Take * (*)(MediaItem *, int)) g_rec->GetFunc("GetTake");
        void (*SetActiveTake)(MediaItem_Take *) =
            (void (*)(MediaItem_Take *))g_rec->GetFunc("SetActiveTake");

        if (CountTakes && GetTake && SetActiveTake) {
          int takeCount = CountTakes(item);
          int takeToDelete = cmd.takeIndex;

          if (takeCount > 1 && takeToDelete < takeCount) {
            // Select only this item for the delete action
            bool (*SetMediaItemSelected)(MediaItem *, bool) = (bool (*)(
                MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
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

            // The rendered take is currently active (REAPER made it active
            // after render) Delete the active take
            Main_OnCommand(40129, 0); // Take: Delete active take from items

            // Set the original take (index 0) as active so user sees original
            MediaItem_Take *originalTake = GetTake(item, 0);
            if (originalTake) {
              SetActiveTake(originalTake);
            }

            if (UpdateArrange) {
              UpdateArrange();
            }
            if (ShowConsoleMsg) {
              ShowConsoleMsg(
                  "MAGDA: Deleted rendered take, restored original\n");
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
    } else if (cmd.type == CMD_DSP_ANALYZE) {
      // Set phase to DSP analysis
      SetCurrentPhase(MIX_PHASE_DSP_ANALYSIS);

      // Check if rendered file is ready (size has stabilized)
      MediaItem *dspItem = (MediaItem *)cmd.itemPtr;
      bool fileReady = false;

      if (dspItem) {
        MediaItem_Take *(*GetActiveTake)(MediaItem *) =
            (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
        PCM_source *(*GetMediaItemTake_Source)(MediaItem_Take *) =
            (PCM_source * (*)(MediaItem_Take *))
                g_rec->GetFunc("GetMediaItemTake_Source");
        void (*GetMediaSourceFileName)(PCM_source *, char *, int) = (void (*)(
            PCM_source *, char *, int))g_rec->GetFunc("GetMediaSourceFileName");

        if (GetActiveTake && GetMediaItemTake_Source &&
            GetMediaSourceFileName) {
          MediaItem_Take *activeTake = GetActiveTake(dspItem);
          if (activeTake) {
            PCM_source *src = GetMediaItemTake_Source(activeTake);
            if (src) {
              char filename[512] = {0};
              GetMediaSourceFileName(src, filename, sizeof(filename));

              if (filename[0]) {
                FILE *f = fopen(filename, "rb");
                if (f) {
                  fseek(f, 0, SEEK_END);
                  long currentSize = ftell(f);
                  fclose(f);

                  if (currentSize > 0 && currentSize == cmd.lastFileSize) {
                    cmd.stableCount++;
                    if (cmd.stableCount >= 3) {
                      fileReady = true;
                      if (ShowConsoleMsg) {
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "MAGDA: File ready (%ld bytes, stable for %d "
                                 "ticks)\n",
                                 currentSize, cmd.stableCount);
                        ShowConsoleMsg(msg);
                      }
                    }
                  } else {
                    cmd.stableCount = 0;
                  }
                  cmd.lastFileSize = currentSize;
                }
              }
            }
          }
        }
      }

      // If file not ready, defer (up to max attempts)
      if (!fileReady && cmd.deferCount > 0) {
        cmd.deferCount--;
        ++it;
        continue; // Skip this command for now, process on next tick
      }

      if (!fileReady) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Warning - proceeding with DSP despite file "
                         "not stabilizing\n");
        }
      }

      // Read audio samples on main thread (audio accessor requires main thread)
      // Then do DSP analysis + API call on background thread
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Reading audio samples on main thread...\n");
      }

      // Configure and read samples (main thread only)
      DSPAnalysisConfig dspConfig;
      dspConfig.fftSize = 4096;
      dspConfig.analyzeFullItem = true;

      RawAudioData audioData =
          MagdaDSPAnalyzer::ReadTrackSamples(cmd.trackIndex, dspConfig);

      if (!audioData.valid || audioData.samples.empty()) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Failed to read audio samples\n");
        }
        // Still queue delete to clean up the rendered take
        ReaperCommand deleteCmd;
        deleteCmd.type = CMD_DELETE_TAKE;
        deleteCmd.trackIndex = cmd.trackIndex;
        deleteCmd.itemPtr = cmd.itemPtr;
        deleteCmd.takeIndex = cmd.takeIndex;
        deleteCmd.completed = false;
        commandsToAdd.push_back(deleteCmd);
      } else {
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg),
                   "MAGDA: Read %zu samples, starting background analysis...\n",
                   audioData.samples.size());
          ShowConsoleMsg(msg);
        }

        // Get FX info on main thread (needs REAPER API)
        WDL_FastString fxJson;
        MagdaDSPAnalyzer::GetTrackFXInfo(cmd.trackIndex, fxJson);
        std::string fxStr = fxJson.Get();

        // Copy data for background thread
        int trackIndex = cmd.trackIndex;
        int selectedTrackIndex = cmd.selectedTrackIndex;
        void *itemPtr = cmd.itemPtr;
        int takeIndex = cmd.takeIndex;
        char trackName[256];
        char trackType[256];
        char userRequest[1024];
        strncpy(trackName, cmd.trackName, sizeof(trackName) - 1);
        trackName[sizeof(trackName) - 1] = '\0';
        strncpy(trackType, cmd.trackType, sizeof(trackType) - 1);
        trackType[sizeof(trackType) - 1] = '\0';
        strncpy(userRequest, cmd.userRequest, sizeof(userRequest) - 1);
        userRequest[sizeof(userRequest) - 1] = '\0';

        // Move audio data to background thread for processing
        std::thread([trackIndex, selectedTrackIndex, itemPtr, takeIndex,
                     trackName, trackType, userRequest, fxStr,
                     audioData = std::move(audioData), dspConfig]() mutable {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

          // Run DSP analysis on background thread
          if (ShowConsoleMsg) {
            ShowConsoleMsg(
                "MAGDA: Running DSP analysis on background thread...\n");
          }

          DSPAnalysisResult analysisResult =
              MagdaDSPAnalyzer::AnalyzeSamples(audioData, dspConfig);

          if (!analysisResult.success) {
            if (ShowConsoleMsg) {
              char msg[512];
              snprintf(msg, sizeof(msg), "MAGDA: DSP analysis failed: %s\n",
                       analysisResult.errorMessage.Get());
              ShowConsoleMsg(msg);
            }
            StoreResult(false, std::string("DSP analysis failed: ") +
                                   analysisResult.errorMessage.Get());
          } else {
            // Convert to JSON
            WDL_FastString analysisJson;
            MagdaDSPAnalyzer::ToJSON(analysisResult, analysisJson);

            // Queue take deletion BEFORE API call (must be done on main thread)
            // We delete the rendered take early to avoid holding onto temp
            // audio
            {
              std::lock_guard<std::mutex> cmdLock(s_commandQueueMutex);
              ReaperCommand deleteCmd;
              deleteCmd.type = CMD_DELETE_TAKE;
              deleteCmd.trackIndex = trackIndex;
              deleteCmd.itemPtr = itemPtr;
              deleteCmd.takeIndex = takeIndex;
              deleteCmd.completed = false;
              s_reaperCommandQueue.push_back(deleteCmd);
            }

            if (ShowConsoleMsg) {
              ShowConsoleMsg(
                  "MAGDA: Queued take deletion, calling Mix API...\n");
            }

            // Send to mix API with TRUE STREAMING
            // Text is streamed to the UI in real-time via
            // MagdaBounceWorkflow::AppendStreamText
            WDL_FastString responseJson, error_msg;
            if (!SendToMixAPI(analysisJson.Get(), fxStr.c_str(),
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
              // Error already reported via CompleteStreaming in SendToMixAPI
            } else {
              if (ShowConsoleMsg) {
                ShowConsoleMsg(
                    "MAGDA: Mix analysis streaming completed successfully!\n");
              }
              // Success already handled by streaming - text was streamed to UI
              // in real-time and CompleteStreaming was called
            }
          }
          // Take deletion already queued before API call
        }).detach();
      }

      cmd.completed = true;
      processedAny = true;
      ++it;
    } else {
      // Unknown command type, remove it
      it = s_reaperCommandQueue.erase(it);
    }
  }

  // Add any new commands that were queued during processing
  // (done after loop to avoid iterator invalidation)
  for (const auto &newCmd : commandsToAdd) {
    s_reaperCommandQueue.push_back(newCmd);
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
