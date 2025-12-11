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

bool MagdaActions::SetTrackSelected(int track_index, bool selected,
                                    WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void (*SetTrackSelected)(MediaTrack *, bool) =
      (void (*)(MediaTrack *, bool))g_rec->GetFunc("SetTrackSelected");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetTrack || !SetTrackSelected) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Set track selection state
  SetTrackSelected(track, selected);

  // Refresh the arrange view to show selection changes
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

bool MagdaActions::SetClipSelected(int track_index, int clip_index,
                                   bool selected, WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  MediaItem *(*GetMediaItem)(ReaProject *, int) =
      (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");
  MediaTrack *(*GetMediaItemTrack)(MediaItem *) =
      (MediaTrack * (*)(MediaItem *)) g_rec->GetFunc("GetMediaItemTrack");
  int (*CountMediaItems)(ReaProject *) =
      (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");
  void (*SetMediaItemSelected)(MediaItem *, bool) =
      (void (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  double (*GetMediaItemInfo_Value)(MediaItem *, const char *) = (double (*)(
      MediaItem *, const char *))g_rec->GetFunc("GetMediaItemInfo_Value");

  if (!GetTrack || !GetMediaItem || !GetMediaItemTrack || !CountMediaItems ||
      !SetMediaItemSelected || !GetMediaItemInfo_Value) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Find the clip on this track
  // REAPER stores all media items globally, so we need to iterate and check
  int total_items = CountMediaItems(nullptr);
  MediaItem *target_item = nullptr;
  int clip_count = 0;

  // Iterate through all media items to find the clip_index-th item on this
  // track
  for (int i = 0; i < total_items; i++) {
    MediaItem *item = GetMediaItem(nullptr, i);
    if (!item) {
      continue;
    }

    MediaTrack *item_track = GetMediaItemTrack(item);
    if (item_track == track) {
      // Found an item on this track
      if (clip_count == clip_index) {
        target_item = item;
        break;
      }
      clip_count++;
    }
  }

  if (!target_item) {
    error_msg.Set("Clip not found on track");
    return false;
  }

  // Set clip selection state
  SetMediaItemSelected(target_item, selected);

  // Refresh the arrange view to show selection changes
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

// Helper function to find clip by position (in seconds) or bar number
static MediaItem *FindClipByPosition(MediaTrack *track, double position,
                                     int bar, WDL_FastString &error_msg) {
  if (!g_rec) {
    return nullptr;
  }

  MediaItem *(*GetMediaItem)(ReaProject *, int) =
      (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");
  MediaTrack *(*GetMediaItemTrack)(MediaItem *) =
      (MediaTrack * (*)(MediaItem *)) g_rec->GetFunc("GetMediaItemTrack");
  int (*CountMediaItems)(ReaProject *) =
      (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");
  double (*GetMediaItemInfo_Value)(MediaItem *, const char *) = (double (*)(
      MediaItem *, const char *))g_rec->GetFunc("GetMediaItemInfo_Value");

  if (!GetMediaItem || !GetMediaItemTrack || !CountMediaItems ||
      !GetMediaItemInfo_Value) {
    return nullptr;
  }

  // Convert bar to position if needed
  double target_position = position;
  if (bar > 0 && position < 0) {
    // Convert bar to time (assuming 4/4 time, 120 BPM = 2 seconds per bar)
    // This is approximate - in production you'd get actual tempo/time sig
    target_position = (bar - 1) * 2.0; // Bar 1 = 0.0, Bar 2 = 2.0, etc.
  }

  if (target_position < 0) {
    return nullptr;
  }

  // Find clip at or near the target position (within 0.1 seconds tolerance)
  int total_items = CountMediaItems(nullptr);
  MediaItem *best_match = nullptr;
  double best_distance = 1.0; // 1 second tolerance

  for (int i = 0; i < total_items; i++) {
    MediaItem *item = GetMediaItem(nullptr, i);
    if (!item) {
      continue;
    }

    MediaTrack *item_track = GetMediaItemTrack(item);
    if (item_track == track) {
      double item_position = GetMediaItemInfo_Value(item, "D_POSITION");
      double distance = fabs(item_position - target_position);

      if (distance < best_distance) {
        best_match = item;
        best_distance = distance;
      }
    }
  }

  return best_match;
}

// Unified track properties setter (Phase 1: Refactoring)
// Sets multiple track properties in a single operation
bool MagdaActions::SetTrackProperties(int track_index, const char *name,
                                      const char *volume_db_str,
                                      const char *pan_str, const char *mute_str,
                                      const char *solo_str,
                                      const char *selected_str,
                                      WDL_FastString &error_msg) {
  // Set name if provided
  if (name && name[0]) {
    if (!SetTrackName(track_index, name, error_msg)) {
      return false;
    }
  }

  // Set volume if provided
  if (volume_db_str && volume_db_str[0]) {
    double volume_db = atof(volume_db_str);
    if (!SetTrackVolume(track_index, volume_db, error_msg)) {
      return false;
    }
  }

  // Set pan if provided
  if (pan_str && pan_str[0]) {
    double pan = atof(pan_str);
    if (!SetTrackPan(track_index, pan, error_msg)) {
      return false;
    }
  }

  // Set mute if provided
  if (mute_str && mute_str[0]) {
    bool mute = (strcmp(mute_str, "true") == 0 || strcmp(mute_str, "1") == 0);
    if (!SetTrackMute(track_index, mute, error_msg)) {
      return false;
    }
  }

  // Set solo if provided
  if (solo_str && solo_str[0]) {
    bool solo = (strcmp(solo_str, "true") == 0 || strcmp(solo_str, "1") == 0);
    if (!SetTrackSolo(track_index, solo, error_msg)) {
      return false;
    }
  }

  // Set selected if provided
  if (selected_str && selected_str[0]) {
    bool selected =
        (strcmp(selected_str, "true") == 0 || strcmp(selected_str, "1") == 0);
    if (!SetTrackSelected(track_index, selected, error_msg)) {
      return false;
    }
  }

  return true;
}

// Unified clip properties setter (Phase 1: Refactoring)
// Sets multiple clip properties in a single operation
// Note: Some clip properties (name, color, position) may need to be
// implemented first
bool MagdaActions::SetClipProperties(int track_index, const char *clip_str,
                                     const char *position_str,
                                     const char *bar_str, const char *name,
                                     const char *color, const char *length_str,
                                     const char *selected_str,
                                     WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  if (!GetTrack) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Find the clip
  MediaItem *target_item = nullptr;
  double new_position = -1.0; // For moving clip

  // If clip index is provided, use it for identification
  // In this case, position_str can be used as the NEW position (for moving)
  if (clip_str) {
    int clip_index = atoi(clip_str);
    // Use existing SetClipSelected helper logic to find by index
    MediaItem *(*GetMediaItem)(ReaProject *, int) =
        (MediaItem * (*)(ReaProject *, int)) g_rec->GetFunc("GetMediaItem");
    MediaTrack *(*GetMediaItemTrack)(MediaItem *) =
        (MediaTrack * (*)(MediaItem *)) g_rec->GetFunc("GetMediaItemTrack");
    int (*CountMediaItems)(ReaProject *) =
        (int (*)(ReaProject *))g_rec->GetFunc("CountMediaItems");

    if (GetMediaItem && GetMediaItemTrack && CountMediaItems) {
      int total_items = CountMediaItems(nullptr);
      int clip_count = 0;

      for (int i = 0; i < total_items; i++) {
        MediaItem *item = GetMediaItem(nullptr, i);
        if (!item) {
          continue;
        }

        MediaTrack *item_track = GetMediaItemTrack(item);
        if (item_track == track) {
          if (clip_count == clip_index) {
            target_item = item;
            break;
          }
          clip_count++;
        }
      }
    }
    // If position_str is provided and we found the clip by index,
    // use position_str as the NEW position (for moving)
    if (target_item && position_str && position_str[0]) {
      new_position = atof(position_str);
    }
  } else {
    // Try position-based or bar-based identification
    if (position_str || bar_str) {
      double position = position_str ? atof(position_str) : -1.0;
      int bar = bar_str ? atoi(bar_str) : -1;
      target_item = FindClipByPosition(track, position, bar, error_msg);
    }
  }

  if (!target_item) {
    error_msg.Set("Clip not found: specify 'clip' (index), 'position' "
                  "(seconds), or 'bar' (bar number)");
    return false;
  }

  // Set properties using REAPER API
  void *(*GetSetMediaItemInfo)(MediaItem *, const char *, void *, bool *) =
      (void *(*)(MediaItem *, const char *, void *, bool *))g_rec->GetFunc(
          "GetSetMediaItemInfo_Value");
  bool (*GetSetMediaItemInfo_String)(MediaItem *, const char *, char *, int) =
      (bool (*)(MediaItem *, const char *, char *, int))g_rec->GetFunc(
          "GetSetMediaItemInfo_String");
  void (*SetMediaItemPosition)(MediaItem *, double, bool) = (void (*)(
      MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemPosition");
  void (*SetMediaItemLength)(MediaItem *, double, bool) =
      (void (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemLength");
  void (*SetMediaItemSelected)(MediaItem *, bool) =
      (void (*)(MediaItem *, bool))g_rec->GetFunc("SetMediaItemSelected");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  // Set name if provided
  if (name && name[0] && GetSetMediaItemInfo_String) {
    GetSetMediaItemInfo_String(target_item, "P_NAME", (char *)name, true);
  }

  // Set color if provided (REAPER uses integer color values)
  if (color && color[0] && GetSetMediaItemInfo) {
    // Parse hex color like "#ff0000" to integer
    // REAPER uses BGR format: 0xBBGGRR
    if (color[0] == '#') {
      unsigned int hex_color = 0;
      if (strlen(color) >= 7) {
        sscanf(color + 1, "%x", &hex_color);
        // Convert RGB to BGR
        unsigned int r = (hex_color >> 16) & 0xFF;
        unsigned int g = (hex_color >> 8) & 0xFF;
        unsigned int b = hex_color & 0xFF;
        unsigned int bgr = (b << 16) | (g << 8) | r;
        int color_val = (int)bgr;
        GetSetMediaItemInfo(target_item, "I_CUSTOMCOLOR", &color_val, nullptr);
      }
    }
  }

  // Set position if provided (for moving clip)
  // new_position is set above if clip was identified by index
  // Otherwise, if we're moving a clip identified by position, we'd need
  // a separate "new_position" field, but for now we'll skip this case
  if (new_position >= 0 && SetMediaItemPosition) {
    SetMediaItemPosition(target_item, new_position, false);
  }

  // Set length if provided
  if (length_str && length_str[0] && SetMediaItemLength) {
    double new_length = atof(length_str);
    SetMediaItemLength(target_item, new_length, false);
  }

  // Set selected if provided
  if (selected_str && selected_str[0] && SetMediaItemSelected) {
    bool selected =
        (strcmp(selected_str, "true") == 0 || strcmp(selected_str, "1") == 0);
    SetMediaItemSelected(target_item, selected);
  }

  // Refresh the arrange view
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

bool MagdaActions::DeleteTrack(int track_index, WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  void (*DeleteTrack)(MediaTrack *) =
      (void (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetTrack || !DeleteTrack) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Delete the track
  DeleteTrack(track);

  // Refresh the arrange view
  if (UpdateArrange) {
    UpdateArrange();
  }

  return true;
}

bool MagdaActions::DeleteClip(int track_index, int clip_index,
                              WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  bool (*DeleteTrackMediaItem)(MediaTrack *, MediaItem *) = (bool (*)(
      MediaTrack *, MediaItem *))g_rec->GetFunc("DeleteTrackMediaItem");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

  if (!GetTrack || !CountTrackMediaItems || !GetTrackMediaItem ||
      !DeleteTrackMediaItem) {
    error_msg.Set("Required REAPER API functions not available");
    return false;
  }

  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    return false;
  }

  // Check if clip index is valid
  int num_items = CountTrackMediaItems(track);
  if (clip_index < 0 || clip_index >= num_items) {
    error_msg.Set("Clip index out of range");
    return false;
  }

  // Get the media item
  MediaItem *item = GetTrackMediaItem(track, clip_index);
  if (!item) {
    error_msg.Set("Clip not found");
    return false;
  }

  // Delete the media item
  if (!DeleteTrackMediaItem(track, item)) {
    error_msg.Set("Failed to delete clip");
    return false;
  }

  // Refresh the arrange view
  if (UpdateArrange) {
    UpdateArrange();
  }

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
  } else if (strcmp(action_type, "set_track") == 0) {
    // Unified track properties action (Phase 1: Refactoring)
    const char *track_str = action->get_string_by_name("track", true);
    if (!track_str) {
      error_msg.Set("Missing 'track' field");
      return false;
    }
    int track_index = atoi(track_str);

    // Get all optional parameters
    const char *name = action->get_string_by_name("name");
    const char *volume_db_str = action->get_string_by_name("volume_db", true);
    const char *pan_str = action->get_string_by_name("pan", true);
    const char *mute_str = action->get_string_by_name("mute", true);
    const char *solo_str = action->get_string_by_name("solo", true);
    const char *selected_str = action->get_string_by_name("selected", true);

    if (SetTrackProperties(track_index, name, volume_db_str, pan_str, mute_str,
                           solo_str, selected_str, error_msg)) {
      result.Append("{\"action\":\"set_track\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "set_clip") == 0) {
    // Unified clip properties action (Phase 1: Refactoring)
    const char *track_str = action->get_string_by_name("track", true);
    if (!track_str) {
      error_msg.Set("Missing 'track' field");
      return false;
    }
    int track_index = atoi(track_str);

    // Get all optional parameters
    const char *clip_str = action->get_string_by_name("clip", true);
    const char *position_str = action->get_string_by_name("position", true);
    const char *bar_str = action->get_string_by_name("bar", true);
    const char *name = action->get_string_by_name("name");
    const char *color = action->get_string_by_name("color");
    const char *length_str = action->get_string_by_name("length", true);
    const char *selected_str = action->get_string_by_name("selected", true);

    // At least one identifier must be provided
    if (!clip_str && !position_str && !bar_str) {
      error_msg.Set("Missing clip identifier: specify 'clip' (index), "
                    "'position' (seconds), or 'bar' (bar number)");
      return false;
    }

    if (SetClipProperties(track_index, clip_str, position_str, bar_str, name,
                          color, length_str, selected_str, error_msg)) {
      result.Append("{\"action\":\"set_clip\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "delete_track") == 0 ||
             strcmp(action_type, "remove_track") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    if (!track_str) {
      error_msg.Set("Missing 'track' field");
      return false;
    }
    int track_index = atoi(track_str);
    if (DeleteTrack(track_index, error_msg)) {
      result.Append("{\"action\":\"delete_track\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "delete_clip") == 0 ||
             strcmp(action_type, "remove_clip") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    if (!track_str) {
      error_msg.Set("Missing 'track' field");
      return false;
    }
    int track_index = atoi(track_str);

    MediaTrack *(*GetTrack)(ReaProject *, int) =
        (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
    bool (*DeleteTrackMediaItem)(MediaTrack *, MediaItem *) = (bool (*)(
        MediaTrack *, MediaItem *))g_rec->GetFunc("DeleteTrackMediaItem");
    void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");

    if (!GetTrack || !DeleteTrackMediaItem) {
      error_msg.Set("Required REAPER API functions not available");
      return false;
    }

    MediaTrack *track = GetTrack(nullptr, track_index);
    if (!track) {
      error_msg.Set("Track not found");
      return false;
    }

    MediaItem *target_item = nullptr;

    // Try to find clip by position/bar first (more reliable), then fall back to
    // index
    const char *position_str = action->get_string_by_name("position", true);
    const char *bar_str = action->get_string_by_name("bar", true);
    const char *clip_str = action->get_string_by_name("clip", true);

    if (position_str || bar_str) {
      double position = position_str ? atof(position_str) : -1.0;
      int bar = bar_str ? atoi(bar_str) : -1;
      target_item = FindClipByPosition(track, position, bar, error_msg);
    }

    // Fall back to index-based deletion
    if (!target_item && clip_str) {
      int clip_index = atoi(clip_str);
      // Use existing DeleteClip for index-based
      if (DeleteClip(track_index, clip_index, error_msg)) {
        result.Append("{\"action\":\"delete_clip\",\"success\":true}");
        return true;
      }
      return false;
    }

    if (!target_item) {
      error_msg.Set("Clip not found: specify 'clip' (index), 'position' "
                    "(seconds), or 'bar' (bar number)");
      return false;
    }

    // Delete the clip
    if (!DeleteTrackMediaItem(track, target_item)) {
      error_msg.Set("Failed to delete clip");
      return false;
    }

    // Refresh the arrange view
    if (UpdateArrange) {
      UpdateArrange();
    }

    result.Append("{\"action\":\"delete_clip\",\"success\":true}");
    return true;
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

  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  // Begin undo block - all MAGDA actions will be grouped as a single undo
  // operation
  void (*Undo_BeginBlock2)(ReaProject *) =
      (void (*)(ReaProject *))g_rec->GetFunc("Undo_BeginBlock2");
  if (Undo_BeginBlock2) {
    Undo_BeginBlock2(nullptr); // nullptr = current project
  }

  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(json, (int)strlen(json));
  if (parser.m_err) {
    error_msg.Set(parser.m_err);
    // End undo block on error
    if (Undo_BeginBlock2) {
      void (*Undo_EndBlock2)(ReaProject *, const char *, int) = (void (*)(
          ReaProject *, const char *, int))g_rec->GetFunc("Undo_EndBlock2");
      if (Undo_EndBlock2) {
        Undo_EndBlock2(nullptr, "MAGDA actions (failed)", 0);
      }
    }
    return false;
  }

  if (!root) {
    error_msg.Set("Failed to parse JSON");
    // End undo block on error
    if (Undo_BeginBlock2) {
      void (*Undo_EndBlock2)(ReaProject *, const char *, int) = (void (*)(
          ReaProject *, const char *, int))g_rec->GetFunc("Undo_EndBlock2");
      if (Undo_EndBlock2) {
        Undo_EndBlock2(nullptr, "MAGDA actions (failed)", 0);
      }
    }
    return false;
  }

  result.Append("{\"results\":[");

  bool success = true;
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
        success = false;
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
      success = false;
    }
  } else {
    error_msg.Set("JSON must be an object or array");
    success = false;
  }

  result.Append("]}");

  // End undo block with description
  void (*Undo_EndBlock2)(ReaProject *, const char *, int) = (void (*)(
      ReaProject *, const char *, int))g_rec->GetFunc("Undo_EndBlock2");
  if (Undo_EndBlock2) {
    const char *desc =
        success ? "MAGDA actions" : "MAGDA actions (partial failure)";
    Undo_EndBlock2(nullptr, desc, 0);
  }

  return success;
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
  // For N bars, we need: start of bar 1 to end of bar N
  // Example: 4 bars = bar 1 start to bar 4 end
  int start_bar = 1;
  int last_bar = bars; // Last bar in the range
  double qn_start = 0.0;
  double qn_end = 0.0;
  int timesig_num = 4;
  int timesig_denom = 4;
  double tempo = 120.0;

  // Get start of first bar (measure 0 = bar 1)
  TimeMap_GetMeasureInfo(nullptr, start_bar - 1, &qn_start, nullptr,
                         &timesig_num, &timesig_denom, &tempo);

  // Get end of last bar by getting qn_end from the last bar's measure
  // Measure index is 0-based, so last_bar - 1
  TimeMap_GetMeasureInfo(nullptr, last_bar - 1, nullptr, &qn_end, nullptr,
                         nullptr, nullptr);

  double time_start = TimeMap2_QNToTime(nullptr, qn_start);
  double time_end = TimeMap2_QNToTime(nullptr, qn_end);
  return time_end - time_start;
}
