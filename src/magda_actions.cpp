#include "magda_actions.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "../WDL/WDL/wdlcstring.h"
#include "reaper_plugin_functions.h"
#include <cmath>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

bool MagdaActions::CreateTrack(int index, const char *name,
                               const char *instrument,
                               WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  void (*InsertTrackInProject)(ReaProject *, int, int) =
      (void (*)(ReaProject *, int, int))g_rec->GetFunc("InsertTrackInProject");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))g_rec->GetFunc(
          "TrackFX_AddByName");

  if (!InsertTrackInProject || !GetSetMediaTrackInfo || !GetTrack) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  // Insert track with default envelopes/FX (flags = 1)
  InsertTrackInProject(nullptr, index, 1);

  // Get the newly created track
  MediaTrack *track = GetTrack(nullptr, index);
  if (!track) {
    error_msg.Set("Failed to get created track");
    return false;
  }

  // Set track name if provided
  if (name && name[0]) {
    GetSetMediaTrackInfo((INT_PTR)track, "P_NAME", (void *)name, nullptr);
  }

  // Add instrument if provided
  if (instrument && instrument[0] && TrackFX_AddByName) {
    // recFX = false (track FX, not input FX), instantiate = -1 (always create
    // new instance)
    int fx_index = TrackFX_AddByName(track, instrument, false, -1);
    if (fx_index < 0) {
      // Log warning but don't fail - track was created successfully
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char log_msg[512];
          snprintf(
              log_msg, sizeof(log_msg),
              "MAGDA: Warning - Failed to add instrument '%s' to track %d\n",
              instrument, index);
          ShowConsoleMsg(log_msg);
        }
      }
    }
  }

  return true;
}

bool MagdaActions::CreateClip(int track_index, double position, double length,
                              WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  MediaItem *(*AddMediaItemToTrack)(MediaTrack *) =
      (MediaItem * (*)(MediaTrack *)) g_rec->GetFunc("AddMediaItemToTrack");
  bool (*SetMediaItemPosition)(MediaItem *, double, bool) = (bool (*)(
      MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemPosition");
  bool (*SetMediaItemLength)(MediaItem *, double, bool) =
      (bool (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemLength");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetTrack || !AddMediaItemToTrack || !SetMediaItemPosition ||
      !SetMediaItemLength) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Create media item
  MediaItem *item = AddMediaItemToTrack(track);
  if (!item) {
    error_msg.Set("Failed to create media item");
    return false;
  }

  // Set position and length
  SetMediaItemPosition(item, position, false);
  SetMediaItemLength(item, length, false);

  // Update UI
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

bool MagdaActions::CreateClipAtBar(int track_index, int bar, int length_bars,
                                   WDL_FastString &error_msg) {
  // Convert bars to time using helper functions
  double position = BarToTime(bar);
  double length = BarsToTime(length_bars);

  // Use existing CreateClip implementation
  return CreateClip(track_index, position, length, error_msg);
}

bool MagdaActions::AddTrackFX(int track_index, const char *fxname, bool recFX,
                              WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))g_rec->GetFunc(
          "TrackFX_AddByName");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetTrack || !TrackFX_AddByName) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  if (!fxname || !fxname[0]) {
    error_msg.Set("FX name cannot be empty");
    return false;
  }

  // Get FX count before adding (for verification)
  int (*TrackFX_GetCount)(MediaTrack *, bool) =
      (int (*)(MediaTrack *, bool))g_rec->GetFunc("TrackFX_GetCount");
  int fx_count_before = 0;
  if (TrackFX_GetCount) {
    fx_count_before = TrackFX_GetCount(track, recFX);
  }

  // Add FX - instantiate = -1 means always create new instance
  int fx_index = TrackFX_AddByName(track, fxname, recFX, -1);
  if (fx_index < 0) {
    error_msg.Set("Failed to add FX: ");
    error_msg.Append(fxname);
    error_msg.Append(" (FX may not be installed or name format incorrect)");
    return false;
  }

  // Verify FX was actually added by checking count
  if (TrackFX_GetCount) {
    int fx_count_after = TrackFX_GetCount(track, recFX);
    if (fx_count_after <= fx_count_before) {
      error_msg.Set(
          "FX add reported success but FX count did not increase. Name: ");
      error_msg.Append(fxname);
      return false;
    }
  }

  // Update UI
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

bool MagdaActions::SetTrackVolume(int track_index, double volume_db,
                                  WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Convert dB to linear volume
  double volume = pow(10.0, volume_db / 20.0);

  // Set volume
  GetSetMediaTrackInfo((INT_PTR)track, "D_VOL", &volume, nullptr);

  return true;
}

bool MagdaActions::SetTrackPan(int track_index, double pan,
                               WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Clamp pan to -1.0 to 1.0
  if (pan < -1.0)
    pan = -1.0;
  if (pan > 1.0)
    pan = 1.0;

  // Set pan
  GetSetMediaTrackInfo((INT_PTR)track, "D_PAN", &pan, nullptr);

  return true;
}

bool MagdaActions::SetTrackMute(int track_index, bool mute,
                                WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  int mute_val = mute ? 1 : 0;
  GetSetMediaTrackInfo((INT_PTR)track, "B_MUTE", &mute_val, nullptr);

  return true;
}

bool MagdaActions::SetTrackSolo(int track_index, bool solo,
                                WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  int solo_val = solo ? 1 : 0;
  GetSetMediaTrackInfo((INT_PTR)track, "I_SOLO", &solo_val, nullptr);

  return true;
}

bool MagdaActions::SetTrackName(int track_index, const char *name,
                                WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
      (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaTrackInfo");

  if (!GetTrack || !GetSetMediaTrackInfo) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  if (!name) {
    error_msg.Set("Name cannot be null");
    return false;
  }

  // Set track name
  GetSetMediaTrackInfo((INT_PTR)track, "P_NAME", (void *)name, nullptr);

  return true;
}

bool MagdaActions::ExecuteAction(const wdl_json_element *action,
                                 WDL_FastString &result,
                                 WDL_FastString &error_msg) {
  if (!action || !action->is_object()) {
    error_msg.Set("Action must be an object");
    return false;
  }

  const char *action_type = action->get_string_by_name("action");
  if (!action_type) {
    error_msg.Set("Missing 'action' field");
    return false;
  }

  if (strcmp(action_type, "create_track") == 0) {
    const char *index_str = action->get_string_by_name("index");
    const char *name = action->get_string_by_name("name");
    const char *instrument = action->get_string_by_name("instrument");
    int index = index_str ? atoi(index_str) : -1;
    if (index < 0) {
      // Default to end of track list
      int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
      if (GetNumTracks) {
        index = GetNumTracks();
      } else {
        index = 0;
      }
    }
    if (CreateTrack(index, name, instrument, error_msg)) {
      result.Append("{\"action\":\"create_track\",\"success\":true,\"index\":");
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", index);
      result.Append(buf);
      result.Append("}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "create_clip") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *position_str = action->get_string_by_name("position", true);
    const char *length_str = action->get_string_by_name("length", true);
    if (!track_str || !position_str || !length_str) {
      error_msg.Set("Missing 'track', 'position', or 'length' field");
      return false;
    }
    int track_index = atoi(track_str);
    double position = atof(position_str);
    double length = atof(length_str);
    if (CreateClip(track_index, position, length, error_msg)) {
      result.Append("{\"action\":\"create_clip\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "create_clip_at_bar") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *bar_str = action->get_string_by_name("bar", true);
    const char *length_bars_str =
        action->get_string_by_name("length_bars", true);
    if (!track_str || !bar_str) {
      error_msg.Set("Missing 'track' or 'bar' field");
      return false;
    }
    int track_index = atoi(track_str);
    int bar = atoi(bar_str);
    int length_bars =
        length_bars_str ? atoi(length_bars_str) : 4; // Default to 4 bars
    if (CreateClipAtBar(track_index, bar, length_bars, error_msg)) {
      result.Append("{\"action\":\"create_clip_at_bar\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "add_track_fx") == 0 ||
             strcmp(action_type, "add_instrument") == 0) {
    // Get track - can be string or number, so use allow_unquoted=true
    const char *track_str = action->get_string_by_name("track", true);
    const char *fxname = action->get_string_by_name("fxname");
    const char *recFX_str = action->get_string_by_name("recFX", true);

    if (!track_str || !fxname) {
      error_msg.Set("Missing 'track' or 'fxname' field");
      return false;
    }
    int track_index = atoi(track_str);
    bool recFX = recFX_str && (strcmp(recFX_str, "true") == 0 ||
                               strcmp(recFX_str, "1") == 0);

    if (AddTrackFX(track_index, fxname, recFX, error_msg)) {
      result.Append("{\"action\":\"");
      result.Append(action_type);
      result.Append("\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_track_name") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *name = action->get_string_by_name("name");
    if (!track_str || !name) {
      error_msg.Set("Missing 'track' or 'name' field");
      return false;
    }
    int track_index = atoi(track_str);
    if (SetTrackName(track_index, name, error_msg)) {
      result.Append("{\"action\":\"set_track_name\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_track_volume") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *volume_str = action->get_string_by_name("volume_db", true);
    if (!track_str || !volume_str) {
      error_msg.Set("Missing 'track' or 'volume_db' field");
      return false;
    }
    int track_index = atoi(track_str);
    double volume_db = atof(volume_str);
    if (SetTrackVolume(track_index, volume_db, error_msg)) {
      result.Append("{\"action\":\"set_track_volume\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_track_pan") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *pan_str = action->get_string_by_name("pan", true);
    if (!track_str || !pan_str) {
      error_msg.Set("Missing 'track' or 'pan' field");
      return false;
    }
    int track_index = atoi(track_str);
    double pan = atof(pan_str);
    if (SetTrackPan(track_index, pan, error_msg)) {
      result.Append("{\"action\":\"set_track_pan\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_track_mute") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *mute_str = action->get_string_by_name("mute", true);
    if (!track_str || !mute_str) {
      error_msg.Set("Missing 'track' or 'mute' field");
      return false;
    }
    int track_index = atoi(track_str);
    bool mute = (strcmp(mute_str, "true") == 0 || strcmp(mute_str, "1") == 0);
    if (SetTrackMute(track_index, mute, error_msg)) {
      result.Append("{\"action\":\"set_track_mute\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_track_solo") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    const char *solo_str = action->get_string_by_name("solo", true);
    if (!track_str || !solo_str) {
      error_msg.Set("Missing 'track' or 'solo' field");
      return false;
    }
    int track_index = atoi(track_str);
    bool solo = (strcmp(solo_str, "true") == 0 || strcmp(solo_str, "1") == 0);
    if (SetTrackSolo(track_index, solo, error_msg)) {
      result.Append("{\"action\":\"set_track_solo\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "add_automation") == 0) {
    // Automation envelope action - supports curve-based and point-based syntax
    const char *track_str = action->get_string_by_name("track", true);
    const char *param = action->get_string_by_name("param");
    if (!track_str || !param) {
      error_msg.Set("Missing 'track' or 'param' field");
      return false;
    }
    int track_index = atoi(track_str);

    // Parse curve-based parameters
    const char *curve = action->get_string_by_name("curve");
    const char *start_str = action->get_string_by_name("start", true);
    const char *end_str = action->get_string_by_name("end", true);
    const char *start_bar_str = action->get_string_by_name("start_bar", true);
    const char *end_bar_str = action->get_string_by_name("end_bar", true);
    const char *from_str = action->get_string_by_name("from", true);
    const char *to_str = action->get_string_by_name("to", true);
    const char *freq_str = action->get_string_by_name("freq", true);
    const char *amplitude_str = action->get_string_by_name("amplitude", true);
    const char *phase_str = action->get_string_by_name("phase", true);
    const char *shape_str = action->get_string_by_name("shape", true);
    wdl_json_element *points_array = action->get_item_by_name("points");

    // Calculate start/end times
    double start_time = 0.0;
    double end_time = 4.0; // Default 4 beats
    if (start_str)
      start_time = atof(start_str);
    if (end_str)
      end_time = atof(end_str);
    if (start_bar_str)
      start_time = BarToTime(atoi(start_bar_str));
    if (end_bar_str)
      end_time = BarToTime(atoi(end_bar_str));

    // Parse optional parameters
    double from_val = -60.0; // Default for volume fade_in
    double to_val = 0.0;
    double freq = 1.0;
    double amplitude = 1.0;
    double phase = 0.0;
    int shape = 0; // Linear

    if (from_str)
      from_val = atof(from_str);
    if (to_str)
      to_val = atof(to_str);
    if (freq_str)
      freq = atof(freq_str);
    if (amplitude_str)
      amplitude = atof(amplitude_str);
    if (phase_str)
      phase = atof(phase_str);
    if (shape_str)
      shape = atoi(shape_str);

    if (AddAutomation(track_index, param, curve, start_time, end_time, from_val,
                      to_val, freq, amplitude, phase, shape, points_array,
                      error_msg)) {
      result.Append(
          "{\"action\":\"add_automation\",\"success\":true,\"param\":\"");
      result.Append(param);
      if (curve) {
        result.Append("\",\"curve\":\"");
        result.Append(curve);
      }
      result.Append("\"}");
      return true;
    }
    return false;
  } else {
    error_msg.Set("Unknown action type");
    return false;
  }
}

bool MagdaActions::ExecuteActions(const char *json, WDL_FastString &result,
                                  WDL_FastString &error_msg) {
  if (!json || !json[0]) {
    error_msg.Set("Empty JSON input");
    return false;
  }

  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(json, (int)strlen(json));
  if (parser.m_err) {
    error_msg.Set(parser.m_err);
    return false;
  }

  if (!root) {
    error_msg.Set("Failed to parse JSON");
    return false;
  }

  result.Append("{\"results\":[");

  // Check if it's an array of actions or a single action
  if (root->is_array()) {
    int num_actions = 0;
    wdl_json_element *item = root->enum_item(0);
    while (item) {
      if (num_actions > 0)
        result.Append(",");

      WDL_FastString action_result, action_error;
      if (ExecuteAction(item, action_result, action_error)) {
        result.Append(action_result.Get());
      } else {
        result.Append("{\"error\":");
        result.Append("\"");
        result.Append(action_error.Get());
        result.Append("\"}");
      }
      num_actions++;
      item = root->enum_item(num_actions);
    }
  } else if (root->is_object()) {
    // Single action
    WDL_FastString action_result, action_error;
    if (ExecuteAction(root, action_result, action_error)) {
      result.Append(action_result.Get());
    } else {
      result.Append("{\"error\":");
      result.Append("\"");
      result.Append(action_error.Get());
      result.Append("\"}");
    }
  } else {
    error_msg.Set("JSON must be an object or array");
    return false;
  }

  result.Append("]}");
  return true;
}

// Helper function: Convert bar number (1-based) to time position
double MagdaActions::BarToTime(int bar) {
  if (!g_rec) {
    return 0.0;
  }

  double (*TimeMap_GetMeasureInfo)(ReaProject *, int, double *, double *, int *,
                                   int *, double *) =
      (double (*)(ReaProject *, int, double *, double *, int *, int *,
                  double *))g_rec->GetFunc("TimeMap_GetMeasureInfo");
  double (*TimeMap2_QNToTime)(ReaProject *, double) =
      (double (*)(ReaProject *, double))g_rec->GetFunc("TimeMap2_QNToTime");

  if (!TimeMap_GetMeasureInfo || !TimeMap2_QNToTime) {
    return 0.0;
  }

  // Bar is 1-based, measure is 0-based
  int measure = bar - 1;
  double qn_start = 0.0;
  double qn_end = 0.0;
  int timesig_num = 4;
  int timesig_denom = 4;
  double tempo = 120.0;

  TimeMap_GetMeasureInfo(nullptr, measure, &qn_start, &qn_end, &timesig_num,
                         &timesig_denom, &tempo);
  return TimeMap2_QNToTime(nullptr, qn_start);
}

// Helper function: Convert number of bars to time duration
double MagdaActions::BarsToTime(int bars) {
  if (!g_rec) {
    return 0.0;
  }

  double (*TimeMap_GetMeasureInfo)(ReaProject *, int, double *, double *, int *,
                                   int *, double *) =
      (double (*)(ReaProject *, int, double *, double *, int *, int *,
                  double *))g_rec->GetFunc("TimeMap_GetMeasureInfo");
  double (*TimeMap2_QNToTime)(ReaProject *, double) =
      (double (*)(ReaProject *, double))g_rec->GetFunc("TimeMap2_QNToTime");

  if (!TimeMap_GetMeasureInfo || !TimeMap2_QNToTime) {
    return 0.0;
  }

  // Get QN for start of first bar and end of last bar
  int start_bar = 1;
  int end_bar = bars;
  double qn_start = 0.0;
  double qn_end = 0.0;
  int timesig_num = 4;
  int timesig_denom = 4;
  double tempo = 120.0;

  TimeMap_GetMeasureInfo(nullptr, start_bar - 1, &qn_start, nullptr,
                         &timesig_num, &timesig_denom, &tempo);
  TimeMap_GetMeasureInfo(nullptr, end_bar, nullptr, &qn_end, nullptr, nullptr,
                         nullptr);

  double time_start = TimeMap2_QNToTime(nullptr, qn_start);
  double time_end = TimeMap2_QNToTime(nullptr, qn_end);
  return time_end - time_start;
}

// Add automation envelope to track
// Supports curve-based and point-based syntax
// Curve types: fade_in, fade_out, ramp, sine, saw, square, exp_in, exp_out
bool MagdaActions::AddAutomation(int track_index, const char *param,
                                 const char *curve, double start_time,
                                 double end_time, double from_val,
                                 double to_val, double freq, double amplitude,
                                 double phase, int shape,
                                 wdl_json_element *points_array,
                                 WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  // Get REAPER API functions
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  TrackEnvelope *(*GetTrackEnvelopeByName)(MediaTrack *, const char *) =
      (TrackEnvelope * (*)(MediaTrack *, const char *))
          g_rec->GetFunc("GetTrackEnvelopeByName");
  bool (*InsertEnvelopePoint)(TrackEnvelope *, double, double, int, double,
                              bool, bool *) =
      (bool (*)(TrackEnvelope *, double, double, int, double, bool,
                bool *))g_rec->GetFunc("InsertEnvelopePoint");
  bool (*Envelope_SortPoints)(TrackEnvelope *) =
      (bool (*)(TrackEnvelope *))g_rec->GetFunc("Envelope_SortPoints");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  double (*TimeMap2_QNToTime)(ReaProject *, double) =
      (double (*)(ReaProject *, double))g_rec->GetFunc("TimeMap2_QNToTime");
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (!GetTrack || !GetTrackEnvelopeByName || !InsertEnvelopePoint) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Get envelope by parameter name
  // Map param names to REAPER envelope names
  const char *envelope_name = nullptr;
  bool is_volume = false;
  bool is_pan = false;

  if (strcmp(param, "volume") == 0) {
    envelope_name = "Volume";
    is_volume = true;
  } else if (strcmp(param, "pan") == 0) {
    envelope_name = "Pan";
    is_pan = true;
  } else if (strcmp(param, "mute") == 0) {
    envelope_name = "Mute";
  } else {
    // FX parameter - format is "FXName:ParamName"
    // For now, only support volume/pan - FX params need more complex handling
    error_msg.Set(
        "FX parameter automation not yet supported - use 'volume' or 'pan'");
    return false;
  }

  TrackEnvelope *envelope = GetTrackEnvelopeByName(track, envelope_name);
  if (!envelope) {
    error_msg.Set("Could not get envelope for parameter: ");
    error_msg.Append(param);
    error_msg.Append(" (envelope may need to be visible/armed)");
    return false;
  }

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddAutomation: track=%d, param=%s, curve=%s, start=%.2f, "
             "end=%.2f\n",
             track_index, param, curve ? curve : "points", start_time,
             end_time);
    ShowConsoleMsg(log_msg);
  }

  // Convert beat times to project time if needed
  double start_sec = start_time;
  double end_sec = end_time;
  if (TimeMap2_QNToTime) {
    // Assume start/end are in beats (quarter notes)
    start_sec = TimeMap2_QNToTime(nullptr, start_time);
    end_sec = TimeMap2_QNToTime(nullptr, end_time);
  }

  // Generate points based on curve type or use provided points
  bool noSort = true;
  int points_inserted = 0;

  if (curve && curve[0]) {
    // Curve-based automation
    double duration = end_sec - start_sec;
    if (duration <= 0) {
      error_msg.Set("End time must be greater than start time");
      return false;
    }

    // Set default from/to values based on curve type
    if (strcmp(curve, "fade_in") == 0) {
      from_val = is_volume ? 0.0 : 0.0; // 0.0 = -inf dB for volume envelope
      to_val = is_volume ? 1.0 : 0.0;   // 1.0 = 0 dB
    } else if (strcmp(curve, "fade_out") == 0) {
      from_val = is_volume ? 1.0 : 0.0;
      to_val = is_volume ? 0.0 : 0.0;
    }

    // For volume envelope, convert from/to values if they look like dB
    if (is_volume) {
      // If from/to are in reasonable dB range, convert to linear
      // REAPER volume envelope: 0.0 = -inf, 1.0 = 0 dB, 2.0 = +6 dB
      if (from_val < 0 || from_val > 2.0) {
        // Convert dB to linear: linear = 10^(dB/20)
        // But cap at some reasonable range
        if (from_val <= -60)
          from_val = 0.0;
        else
          from_val = pow(10.0, from_val / 20.0);
      }
      if (to_val < 0 || to_val > 2.0) {
        if (to_val <= -60)
          to_val = 0.0;
        else
          to_val = pow(10.0, to_val / 20.0);
      }
    }

    // Generate points for the curve
    int num_points = 32; // Resolution for curve generation

    for (int i = 0; i <= num_points; i++) {
      double t = (double)i / num_points; // 0.0 to 1.0
      double time_pos = start_sec + t * duration;
      double value = 0.0;

      if (strcmp(curve, "fade_in") == 0 || strcmp(curve, "ramp") == 0) {
        // Linear interpolation
        value = from_val + t * (to_val - from_val);
      } else if (strcmp(curve, "fade_out") == 0) {
        // Linear interpolation
        value = from_val + t * (to_val - from_val);
      } else if (strcmp(curve, "exp_in") == 0) {
        // Exponential ease-in: slow start, fast end
        double exp_t = t * t;
        value = from_val + exp_t * (to_val - from_val);
      } else if (strcmp(curve, "exp_out") == 0) {
        // Exponential ease-out: fast start, slow end
        double exp_t = 1.0 - (1.0 - t) * (1.0 - t);
        value = from_val + exp_t * (to_val - from_val);
      } else if (strcmp(curve, "sine") == 0) {
        // Sinusoidal oscillation
        double center = is_pan ? 0.0 : 0.5;
        double osc = sin(2.0 * M_PI * (freq * t + phase));
        value = center + amplitude * osc * (is_pan ? 1.0 : 0.5);
      } else if (strcmp(curve, "saw") == 0) {
        // Sawtooth wave
        double center = is_pan ? 0.0 : 0.5;
        double saw_phase = fmod(freq * t + phase, 1.0);
        double osc = 2.0 * saw_phase - 1.0; // -1 to 1
        value = center + amplitude * osc * (is_pan ? 1.0 : 0.5);
      } else if (strcmp(curve, "square") == 0) {
        // Square wave
        double center = is_pan ? 0.0 : 0.5;
        double sq_phase = fmod(freq * t + phase, 1.0);
        double osc = sq_phase < 0.5 ? 1.0 : -1.0;
        value = center + amplitude * osc * (is_pan ? 1.0 : 0.5);
      } else {
        // Unknown curve - use linear ramp as fallback
        value = from_val + t * (to_val - from_val);
      }

      // Clamp value to valid range
      if (is_volume) {
        if (value < 0.0)
          value = 0.0;
        if (value > 4.0)
          value = 4.0; // ~12 dB max
      } else if (is_pan) {
        if (value < -1.0)
          value = -1.0;
        if (value > 1.0)
          value = 1.0;
      }

      if (InsertEnvelopePoint(envelope, time_pos, value, shape, 0.0, false,
                              &noSort)) {
        points_inserted++;
      }
    }
  } else if (points_array && points_array->is_array()) {
    // Point-based automation
    int i = 0;
    wdl_json_element *point_obj = points_array->enum_item(i);
    while (point_obj) {
      if (point_obj->is_object()) {
        const char *time_str = point_obj->get_string_by_name("time", true);
        const char *bar_str = point_obj->get_string_by_name("bar", true);
        const char *value_str = point_obj->get_string_by_name("value", true);

        if (value_str) {
          double time_pos = 0.0;
          if (time_str) {
            time_pos = atof(time_str);
            if (TimeMap2_QNToTime) {
              time_pos = TimeMap2_QNToTime(nullptr, time_pos);
            }
          } else if (bar_str) {
            time_pos = BarToTime(atoi(bar_str));
          }

          double value = atof(value_str);

          // Convert dB to linear for volume
          if (is_volume && (value < 0 || value > 2.0)) {
            if (value <= -60)
              value = 0.0;
            else
              value = pow(10.0, value / 20.0);
          }

          if (InsertEnvelopePoint(envelope, time_pos, value, shape, 0.0, false,
                                  &noSort)) {
            points_inserted++;
          }
        }
      }
      i++;
      point_obj = points_array->enum_item(i);
    }
  } else {
    error_msg.Set("add_automation: must specify 'curve' or 'points'");
    return false;
  }

  // Sort envelope points
  if (points_inserted > 0 && Envelope_SortPoints) {
    Envelope_SortPoints(envelope);
  }

  // Update UI
  if (UpdateArrange) {
    UpdateArrange();
  }

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddAutomation: SUCCESS - Inserted %d envelope points\n",
             points_inserted);
    ShowConsoleMsg(log_msg);
  }

  return points_inserted > 0;
}
