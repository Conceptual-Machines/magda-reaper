#include "magda_state.h"
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

void MagdaState::GetTracksInfo(WDL_FastString &json) {
  json.Append("\"tracks\":[");

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  if (!GetNumTracks) {
    json.Append("]");
    return;
  }

  int numTracks = GetNumTracks();
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  const char *(*GetTrackInfo)(INT_PTR, int *) =
      (const char *(*)(INT_PTR, int *))g_rec->GetFunc("GetTrackInfo");
  bool (*GetTrackUIVolPan)(MediaTrack *, double *, double *) = (bool (*)(
      MediaTrack *, double *, double *))g_rec->GetFunc("GetTrackUIVolPan");
  bool (*GetTrackUIMute)(MediaTrack *, bool *) =
      (bool (*)(MediaTrack *, bool *))g_rec->GetFunc("GetTrackUIMute");

  for (int i = 0; i < numTracks; i++) {
    if (i > 0)
      json.Append(",");

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

  GetTracksInfo(json);

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
