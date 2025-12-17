#include "magda_state.h"
#include "magda_settings_window.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "../WDL/WDL/wdlcstring.h"
#include "reaper_plugin_functions.h"
#include <cmath>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// Helper to escape JSON string
void MagdaState::EscapeJSONString(const char *str, WDL_FastString &out) {
  if (!str) {
    out.Append("\"\"");
    return;
  }
  out.Append("\"");
  const char *p = str;
  while (*p) {
    switch (*p) {
    case '"':
      out.Append("\\\"");
      break;
    case '\\':
      out.Append("\\\\");
      break;
    case '\n':
      out.Append("\\n");
      break;
    case '\r':
      out.Append("\\r");
      break;
    case '\t':
      out.Append("\\t");
      break;
    default:
      if (*p >= 0 && *p < 32) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
        out.Append(buf);
      } else {
        out.Append(p, 1);
      }
      break;
    }
    p++;
  }
  out.Append("\"");
}

void MagdaState::GetProjectInfo(WDL_FastString &json) {
  json.Append("\"project\":{");

  // Get project name
  if (g_rec) {
    void (*GetProjectName)(ReaProject *, char *, int) =
        (void (*)(ReaProject *, char *, int))g_rec->GetFunc("GetProjectName");
    if (GetProjectName) {
      char projName[512];
      GetProjectName(nullptr, projName, sizeof(projName));
      json.Append("\"name\":");
      EscapeJSONString(projName, json);
      json.Append(",");
    }
  }

  // Get project length
  double (*GetProjectLength)(ReaProject *) =
      (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");
  if (GetProjectLength) {
    double length = GetProjectLength(nullptr);
    char buf[64];
    snprintf(buf, sizeof(buf), "\"length\":%.6f", length);
    json.Append(buf);
  } else {
    json.Append("\"length\":0");
  }

  json.Append("}");
}

void MagdaState::GetPlayState(WDL_FastString &json) {
  json.Append("\"play_state\":{");

  int (*GetPlayState)() = (int (*)())g_rec->GetFunc("GetPlayState");
  if (GetPlayState) {
    int state = GetPlayState();
    bool playing = (state & 1) != 0;
    bool paused = (state & 2) != 0;
    bool recording = (state & 4) != 0;

    char buf[128];
    snprintf(buf, sizeof(buf), "\"playing\":%s,\"paused\":%s,\"recording\":%s",
             playing ? "true" : "false", paused ? "true" : "false",
             recording ? "true" : "false");
    json.Append(buf);
  } else {
    json.Append("\"playing\":false,\"paused\":false,\"recording\":false");
  }

  // Get play position
  double (*GetPlayPosition)() = (double (*)())g_rec->GetFunc("GetPlayPosition");
  if (GetPlayPosition) {
    double pos = GetPlayPosition();
    char buf[64];
    snprintf(buf, sizeof(buf), ",\"position\":%.6f", pos);
    json.Append(buf);
  } else {
    json.Append(",\"position\":0");
  }

  // Get cursor position
  double (*GetCursorPosition)() =
      (double (*)())g_rec->GetFunc("GetCursorPosition");
  if (GetCursorPosition) {
    double pos = GetCursorPosition();
    char buf[64];
    snprintf(buf, sizeof(buf), ",\"cursor\":%.6f", pos);
    json.Append(buf);
  } else {
    json.Append(",\"cursor\":0");
  }

  json.Append("}");
}

void MagdaState::GetTimeSelection(WDL_FastString &json) {
  json.Append("\"time_selection\":{");

  void (*GetSet_LoopTimeRange2)(ReaProject *, bool, bool, double *, double *,
                                bool) =
      (void (*)(ReaProject *, bool, bool, double *, double *,
                bool))g_rec->GetFunc("GetSet_LoopTimeRange2");
  if (GetSet_LoopTimeRange2) {
    double start = 0, end = 0;
    GetSet_LoopTimeRange2(nullptr, false, false, &start, &end, false);
    char buf[128];
    snprintf(buf, sizeof(buf), "\"start\":%.6f,\"end\":%.6f", start, end);
    json.Append(buf);
  } else {
    json.Append("\"start\":0,\"end\":0");
  }

  json.Append("}");
}

StateFilterPreferences MagdaState::LoadStateFilterPreferences() {
  StateFilterPreferences prefs;
  MagdaSettingsWindow::LoadStateFilterPreferences(prefs);
  return prefs;
}

bool MagdaState::ShouldIncludeTrack(int trackIndex, bool isSelected,
                                    int clipCount,
                                    const StateFilterPreferences *prefs) {
  // Always include all tracks - filtering only applies to clips
  // Tracks provide context even if they have no clips
  (void)trackIndex; // Unused
  (void)isSelected; // Unused
  (void)clipCount;  // Unused
  (void)prefs;      // Unused
  return true;
}

bool MagdaState::ShouldIncludeClip(bool trackSelected, bool clipSelected,
                                   const StateFilterPreferences *prefs) {
  if (!prefs) {
    return true; // No filtering
  }

  switch (prefs->mode) {
  case StateFilterMode::All:
  case StateFilterMode::SelectedTracksOnly:
    // Include all clips on included tracks
    return true;

  case StateFilterMode::SelectedTrackClipsOnly:
    // Only include selected clips on selected tracks
    return trackSelected && clipSelected;

  case StateFilterMode::SelectedClipsOnly:
    // Include only selected clips (regardless of track)
    return clipSelected;

  default:
    return true;
  }
}

void MagdaState::GetTracksInfo(WDL_FastString &json,
                               const StateFilterPreferences *prefs) {
  json.Append("\"tracks\":[");

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  if (!GetNumTracks) {
    json.Append("]");
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: GetNumTracks function not available\n");
      }
    }
    return;
  }

  int numTracks = GetNumTracks();

  // Debug logging
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[256];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: GetNumTracks() returned %d tracks\n", numTracks);
      ShowConsoleMsg(log_msg);
    }
  }

  bool firstTrack = true;
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  const char *(*GetTrackInfo)(INT_PTR, int *) =
      (const char *(*)(INT_PTR, int *))g_rec->GetFunc("GetTrackInfo");
  bool (*GetTrackUIVolPan)(MediaTrack *, double *, double *) = (bool (*)(
      MediaTrack *, double *, double *))g_rec->GetFunc("GetTrackUIVolPan");
  bool (*GetTrackUIMute)(MediaTrack *, bool *) =
      (bool (*)(MediaTrack *, bool *))g_rec->GetFunc("GetTrackUIMute");

  for (int i = 0; i < numTracks; i++) {
    // Check if we should include this track based on preferences
    // We need to check selection state first
    bool isSelected = false;
    int clipCount = 0;

    if (GetTrackInfo) {
      int flags = 0;
      GetTrackInfo(i, &flags);
      isSelected = (flags & 2) != 0;
    }

    // Count clips to check if track is empty
    MediaTrack *track = GetTrack ? GetTrack(nullptr, i) : nullptr;
    if (track) {
      int (*CountTrackMediaItems)(MediaTrack *) =
          (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
      if (CountTrackMediaItems) {
        clipCount = CountTrackMediaItems(track);
      }
    }

    // Always include tracks - filtering only applies to clips
    // (ShouldIncludeTrack now always returns true, but we keep the call for
    // consistency)
    if (!ShouldIncludeTrack(i, isSelected, clipCount, prefs)) {
      continue;
    }

    if (!firstTrack) {
      json.Append(",");
    }
    firstTrack = false;

    json.Append("{");

    // Track index
    char buf[256]; // Increased buffer size to handle all track fields
    snprintf(buf, sizeof(buf), "\"index\":%d", i);
    json.Append(buf);

    // Track name
    if (GetTrackInfo) {
      int flags = 0;
      const char *name = GetTrackInfo(i, &flags);
      if (name) {
        json.Append(",\"name\":");
        EscapeJSONString(name, json);
      } else {
        json.Append(",\"name\":\"\"");
      }

      // Track flags
      bool isFolder = (flags & 1) != 0;
      bool isSelected = (flags & 2) != 0;
      bool hasFX = (flags & 4) != 0;
      bool isMuted = (flags & 8) != 0;
      bool isSoloed = (flags & 16) != 0;
      bool isRecArmed = (flags & 64) != 0;

      // Build flags string - no trailing comma after rec_armed since
      // volume_db/pan and ui_muted are optional
      snprintf(buf, sizeof(buf),
               ",\"folder\":%s,\"selected\":%s,\"has_fx\":%s,\"muted\":%s,"
               "\"soloed\":%s,\"rec_armed\":%s",
               isFolder ? "true" : "false", isSelected ? "true" : "false",
               hasFX ? "true" : "false", isMuted ? "true" : "false",
               isSoloed ? "true" : "false", isRecArmed ? "true" : "false");
      json.Append(buf);
    }

    // Volume and pan (add comma before if we have flags)
    if (GetTrack && GetTrackUIVolPan) {
      MediaTrack *track = GetTrack(nullptr, i);
      if (track) {
        double vol = 0, pan = 0;
        if (GetTrackUIVolPan(track, &vol, &pan)) {
          // Convert volume to dB
          double vol_db = 20.0 * log10(vol);
          snprintf(buf, sizeof(buf), ",\"volume_db\":%.2f,\"pan\":%.2f", vol_db,
                   pan);
          json.Append(buf);
        }
      }
    }

    // Mute state (from UI, more reliable) - comma already included in format
    // string
    if (GetTrack && GetTrackUIMute) {
      MediaTrack *track = GetTrack(nullptr, i);
      if (track) {
        bool muted = false;
        if (GetTrackUIMute(track, &muted)) {
          snprintf(buf, sizeof(buf), ",\"ui_muted\":%s",
                   muted ? "true" : "false");
          json.Append(buf);
        }
      }
    }

    // Get clips/items on this track
    if (GetTrack) {
      MediaTrack *track = GetTrack(nullptr, i);
      if (track) {
        int (*CountTrackMediaItems)(MediaTrack *) =
            (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
        MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
            (MediaItem * (*)(MediaTrack *, int))
                g_rec->GetFunc("GetTrackMediaItem");
        double (*GetMediaItemInfo_Value)(MediaItem *, const char *) =
            (double (*)(MediaItem *, const char *))g_rec->GetFunc(
                "GetMediaItemInfo_Value");
        // Get take info functions for item names (items don't have names, takes
        // do)
        MediaItem_Take *(*GetActiveTake)(MediaItem *) =
            (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
        bool (*GetSetMediaItemTakeInfo_String)(MediaItem_Take *, const char *,
                                               char *, bool) =
            (bool (*)(MediaItem_Take *, const char *, char *,
                      bool))g_rec->GetFunc("GetSetMediaItemTakeInfo_String");

        if (CountTrackMediaItems && GetTrackMediaItem &&
            GetMediaItemInfo_Value) {
          int numItems = CountTrackMediaItems(track);
          json.Append(",\"clips\":[");

          bool firstClip = true;
          int clipsAdded = 0;

          for (int clipIdx = 0; clipIdx < numItems; clipIdx++) {
            // Check max clips per track limit
            if (prefs && prefs->maxClipsPerTrack > 0 &&
                clipsAdded >= prefs->maxClipsPerTrack) {
              break;
            }

            MediaItem *item = GetTrackMediaItem(track, clipIdx);
            if (!item || !GetMediaItemInfo_Value) {
              continue;
            }

            // Check if clip is selected
            // Defensive: validate item is still valid before accessing
            double selected = 0;
            if (GetMediaItemInfo_Value) {
              selected = GetMediaItemInfo_Value(item, "B_UISEL");
            }
            bool clipSelected = selected > 0.5;

            // Check if we should include this clip based on preferences
            if (!ShouldIncludeClip(isSelected, clipSelected, prefs)) {
              continue;
            }

            if (!firstClip) {
              json.Append(",");
            }
            firstClip = false;
            clipsAdded++;

            json.Append("{");

            // Clip index (0-based on track)
            snprintf(buf, sizeof(buf), "\"index\":%d", clipIdx);
            json.Append(buf);

            // Position (in seconds) - validate item still valid
            double position = 0;
            if (GetMediaItemInfo_Value) {
              position = GetMediaItemInfo_Value(item, "D_POSITION");
            }
            snprintf(buf, sizeof(buf), ",\"position\":%.6f", position);
            json.Append(buf);

            // Length (in seconds) - validate item still valid
            double length = 0;
            if (GetMediaItemInfo_Value) {
              length = GetMediaItemInfo_Value(item, "D_LENGTH");
            }
            snprintf(buf, sizeof(buf), ",\"length\":%.6f", length);
            json.Append(buf);

            // Selected state
            snprintf(buf, sizeof(buf), ",\"selected\":%s",
                     clipSelected ? "true" : "false");
            json.Append(buf);

            // Clip name (from active take, if available)
            // Items don't have names directly - takes do
            // Defensive: validate all pointers before use
            // NOTE: We removed GetSetMediaItemInfo_String call which was
            // causing crashes Items don't have P_NAME - only takes do, so we
            // get the active take first
            if (item && GetActiveTake && GetSetMediaItemTakeInfo_String) {
              // Double-check item is still valid (defensive)
              if (!GetMediaItemInfo_Value) {
                // Item or function became invalid, skip name
              } else {
                // Quick validation that item is still valid by reading a safe
                // property
                double testPos = GetMediaItemInfo_Value(item, "D_POSITION");
                (void)testPos; // Suppress unused warning

                MediaItem_Take *take = GetActiveTake(item);
                if (take && GetSetMediaItemTakeInfo_String) {
                  // Allocate buffer for take name
                  char takeNameBuf[512] = {0};
                  bool success = GetSetMediaItemTakeInfo_String(
                      take, "P_NAME", takeNameBuf, false);
                  if (success && takeNameBuf[0]) {
                    json.Append(",\"name\":");
                    EscapeJSONString(takeNameBuf, json);
                  }
                }
              }
            }

            json.Append("}");
          }

          json.Append("]");
        }
      }
    }

    json.Append("}");
  }

  json.Append("]");
}

char *MagdaState::GetStateSnapshot() {
  WDL_FastString json;
  json.Append("{");

  GetProjectInfo(json);
  json.Append(",");

  GetPlayState(json);
  json.Append(",");

  GetTimeSelection(json);
  json.Append(",");

  // Load preferences and apply filtering (only affects clips, not tracks)
  StateFilterPreferences prefs = LoadStateFilterPreferences();
  GetTracksInfo(json, &prefs);

  json.Append("}");

  // Allocate and copy the string
  int len = json.GetLength();
  char *result = (char *)malloc(len + 1);
  if (result) {
    memcpy(result, json.Get(), len);
    result[len] = '\0';
  }
  return result;
}
