#include "magda_actions.h"
#include "magda_drum_mapping.h"
#include "magda_dsp_analyzer.h"
#include "magda_plugin_scanner.h"
// Workaround for typo in reaper_plugin_functions.h line 6475 (Reaproject ->
// ReaProject) This is a typo in the REAPER SDK itself, not our code
typedef ReaProject Reaproject;
#include "../WDL/WDL/wdlcstring.h"
#include "reaper_plugin_functions.h"
#include <cmath>
#include <cstring>
#include <string>

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
    // Resolve plugin alias to full name
    extern MagdaPluginScanner *g_pluginScanner;
    std::string resolved_instrument_str = instrument;
    if (g_pluginScanner) {
      std::string resolved = g_pluginScanner->ResolveAlias(instrument);
      if (!resolved.empty()) {
        resolved_instrument_str = resolved;
      }
    }

    // recFX = false (track FX, not input FX), instantiate = -1 (always create
    // new instance)
    int fx_index =
        TrackFX_AddByName(track, resolved_instrument_str.c_str(), false, -1);
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

  // Resolve plugin alias to full name
  extern MagdaPluginScanner *g_pluginScanner;
  std::string resolved_fxname_str = fxname;
  if (g_pluginScanner) {
    std::string resolved = g_pluginScanner->ResolveAlias(fxname);
    if (!resolved.empty()) {
      resolved_fxname_str = resolved;
    }
  }

  // Get FX count before adding (for verification)
  int (*TrackFX_GetCount)(MediaTrack *, bool) =
      (int (*)(MediaTrack *, bool))g_rec->GetFunc("TrackFX_GetCount");
  int fx_count_before = 0;
  if (TrackFX_GetCount) {
    fx_count_before = TrackFX_GetCount(track, recFX);
  }

  // Add FX - instantiate = -1 means always create new instance
  int fx_index =
      TrackFX_AddByName(track, resolved_fxname_str.c_str(), recFX, -1);
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
                                      const char *color_str,
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

  // Set color if provided (similar to clip color handling)
  if (color_str && color_str[0]) {
    MediaTrack *(*GetTrack)(ReaProject *, int) =
        (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
    void *(*GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *) =
        (void *(*)(INT_PTR, const char *, void *, bool *))g_rec->GetFunc(
            "GetSetMediaTrackInfo");
    void (*SetTrackColor)(MediaTrack *, int) =
        (void (*)(MediaTrack *, int))g_rec->GetFunc("SetTrackColor");

    if (!GetTrack || !GetSetMediaTrackInfo) {
      error_msg.Set("Required REAPER API functions not available for color");
      return false;
    }

    MediaTrack *track = GetTrack(nullptr, track_index);
    if (!track) {
      error_msg.Set("Track not found");
      return false;
    }

    // Parse color (hex like "#ff0000" - parser should convert color names to
    // hex)
    int color_val = 0;
    if (color_str[0] == '#') {
      // Hex color - convert to BGR format (REAPER uses BGR)
      unsigned int hex_color = 0;
      if (strlen(color_str) >= 7) {
        sscanf(color_str + 1, "%x", &hex_color);
        // Convert RGB to BGR
        unsigned int r = (hex_color >> 16) & 0xFF;
        unsigned int g = (hex_color >> 8) & 0xFF;
        unsigned int b = hex_color & 0xFF;
        unsigned int bgr = (b << 16) | (g << 8) | r;
        color_val = (int)bgr;
      }
    } else {
      // Parser should convert color names to hex, but handle as hex anyway
      // Try to parse as hex without #
      unsigned int hex_color = 0;
      if (sscanf(color_str, "%x", &hex_color) == 1) {
        unsigned int r = (hex_color >> 16) & 0xFF;
        unsigned int g = (hex_color >> 8) & 0xFF;
        unsigned int b = hex_color & 0xFF;
        unsigned int bgr = (b << 16) | (g << 8) | r;
        color_val = (int)bgr;
      } else {
        error_msg.Set("Invalid color format - expected hex (e.g., #ff0000)");
        return false;
      }
    }

    // Use SetTrackColor if available, otherwise use GetSetMediaTrackInfo
    if (SetTrackColor) {
      SetTrackColor(track, color_val | 0x1000000); // Set flag bit
    } else {
      int color_with_flag = color_val | 0x1000000;
      GetSetMediaTrackInfo((INT_PTR)track, "I_CUSTOMCOLOR", &color_with_flag,
                           nullptr);
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
    const char *color_str = action->get_string_by_name("color");

    if (SetTrackProperties(track_index, name, volume_db_str, pan_str, mute_str,
                           solo_str, selected_str, color_str, error_msg)) {
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
  } else if (strcmp(action_type, "add_midi") == 0) {
    const char *track_str = action->get_string_by_name("track", true);
    wdl_json_element *notes_array = action->get_item_by_name("notes");
    if (!track_str || !notes_array) {
      error_msg.Set("Missing 'track' or 'notes' field");
      return false;
    }
    int track_index = atoi(track_str);
    if (AddMIDI(track_index, notes_array, error_msg)) {
      result.Append("{\"action\":\"add_midi\",\"success\":true}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "drum_pattern") == 0) {
    const char *drum_name = action->get_string_by_name("drum");
    const char *grid = action->get_string_by_name("grid");
    const char *velocity_str = action->get_string_by_name("velocity", true);
    const char *track_str = action->get_string_by_name("track", true);
    int velocity = velocity_str ? atoi(velocity_str) : 100;

    if (!drum_name || !grid) {
      error_msg.Set("drum_pattern: missing 'drum' or 'grid' field");
      return false;
    }

    // Use specified track, or last track if not specified
    int track_index = 0;
    if (track_str) {
      track_index = atoi(track_str);
    } else {
      // Default to last track (most recently created)
      int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
      if (GetNumTracks) {
        int num_tracks = GetNumTracks();
        if (num_tracks > 0) {
          track_index = num_tracks - 1;
        }
      }
    }

    if (AddDrumPattern(track_index, drum_name, grid, velocity, nullptr,
                       error_msg)) {
      result.Append(
          "{\"action\":\"drum_pattern\",\"success\":true,\"drum\":\"");
      result.Append(drum_name);
      result.Append("\"}");
      return true;
    }
    return false;
  } else if (strcmp(action_type, "analyze_track") == 0) {
    // DSP Analysis action - analyze a track's audio content
    const char *track_str = action->get_string_by_name("track", true);
    if (!track_str) {
      error_msg.Set("Missing 'track' field");
      return false;
    }
    int track_index = atoi(track_str);

    // Optional config parameters
    DSPAnalysisConfig config;
    const char *fft_size_str = action->get_string_by_name("fft_size", true);
    if (fft_size_str) {
      config.fftSize = atoi(fft_size_str);
    }
    const char *max_length_str = action->get_string_by_name("max_length", true);
    if (max_length_str) {
      config.analysisLength = (float)atof(max_length_str);
      config.analyzeFullItem = false;
    }

    // Perform analysis
    DSPAnalysisResult analysisResult =
        MagdaDSPAnalyzer::AnalyzeTrack(track_index, config);

    if (analysisResult.success) {
      result.Append(
          "{\"action\":\"analyze_track\",\"success\":true,\"analysis\":");
      WDL_FastString analysisJson;
      MagdaDSPAnalyzer::ToJSON(analysisResult, analysisJson);
      result.Append(analysisJson.Get());

      // Also include FX info
      result.Append(",");
      MagdaDSPAnalyzer::GetTrackFXInfo(track_index, result);
      result.Append("}");
      return true;
    } else {
      error_msg.Set(analysisResult.errorMessage.Get());
      return false;
    }
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
    // First pass: collect all drum_pattern notes, execute other actions
    WDL_FastString drum_notes_json;
    drum_notes_json.Append("[");
    int drum_note_count = 0;
    int drum_track_index = -1;
    double drum_bar_offset = 0.0; // Track bar position for drum patterns

    int num_actions = 0;
    int result_count = 0;
    wdl_json_element *item = root->enum_item(0);
    while (item) {
      const char *action_type = item->get_string_by_name("action");

      // Capture bar position from create_clip_at_bar for drum patterns
      if (action_type && strcmp(action_type, "create_clip_at_bar") == 0) {
        const char *bar_str = item->get_string_by_name("bar", true);
        if (bar_str) {
          int bar = atoi(bar_str);
          // Convert bar number to beat offset (bar 1 = beat 0, bar 2 = beat 4,
          // etc.)
          drum_bar_offset = (bar - 1) * 4.0;
        }
      }

      // Check if this is a drum_pattern action
      if (action_type && strcmp(action_type, "drum_pattern") == 0) {
        // Collect drum pattern notes instead of executing immediately
        const char *drum_name = item->get_string_by_name("drum");
        const char *grid = item->get_string_by_name("grid");
        const char *velocity_str = item->get_string_by_name("velocity", true);
        const char *track_str = item->get_string_by_name("track", true);
        int velocity = velocity_str ? atoi(velocity_str) : 100;

        // Determine track index
        if (drum_track_index < 0) {
          if (track_str) {
            drum_track_index = atoi(track_str);
          } else {
            int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
            if (GetNumTracks) {
              int num_tracks = GetNumTracks();
              drum_track_index = num_tracks > 0 ? num_tracks - 1 : 0;
            }
          }
        }

        if (drum_name && grid) {
          int midi_note = ResolveDrumNote(drum_name, nullptr);
          if (midi_note >= 0) {
            // Parse grid and add notes to collection
            size_t grid_len = strlen(grid);
            double sixteenth = 0.25;
            for (size_t i = 0; i < grid_len; i++) {
              char c = grid[i];
              int note_vel = 0;
              if (c == 'x')
                note_vel = velocity;
              else if (c == 'X')
                note_vel = 127;
              else if (c == 'o')
                note_vel = 60;
              else
                continue;

              if (drum_note_count > 0)
                drum_notes_json.Append(",");
              char note_json[256];
              // Offset note start by bar position
              double note_start = drum_bar_offset + (i * sixteenth);
              snprintf(
                  note_json, sizeof(note_json),
                  "{\"pitch\":%d,\"velocity\":%d,\"start\":%.4f,\"length\":"
                  "%.4f}",
                  midi_note, note_vel, note_start, sixteenth);
              drum_notes_json.Append(note_json);
              drum_note_count++;
            }
          }
        }
      } else {
        // Execute non-drum_pattern action normally
        if (result_count > 0)
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
        result_count++;
      }
      num_actions++;
      item = root->enum_item(num_actions);
    }

    drum_notes_json.Append("]");

    // Now add all drum notes at once if we collected any
    if (drum_note_count > 0 && drum_track_index >= 0) {
      if (result_count > 0)
        result.Append(",");

      wdl_json_parser drum_parser;
      wdl_json_element *drum_notes_array = drum_parser.parse(
          drum_notes_json.Get(), (int)drum_notes_json.GetLength());

      if (drum_notes_array) {
        WDL_FastString drum_error;
        if (AddMIDI(drum_track_index, drum_notes_array, drum_error)) {
          result.Append(
              "{\"action\":\"drum_pattern\",\"success\":true,\"notes\":"
              "");
          char buf[32];
          snprintf(buf, sizeof(buf), "%d", drum_note_count);
          result.Append(buf);
          result.Append("}");
        } else {
          result.Append("{\"error\":\"");
          result.Append(drum_error.Get());
          result.Append("\"}");
          success = false;
        }
      }
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

bool MagdaActions::AddMIDI(int track_index, wdl_json_element *notes_array,
                           WDL_FastString &error_msg) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI called: track_index=%d\n", track_index);
    ShowConsoleMsg(log_msg);
  }

  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: REAPER API not available\n");
    return false;
  }

  if (!notes_array || !notes_array->is_array()) {
    error_msg.Set("'notes' must be an array");
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: notes must be an array\n");
    return false;
  }

  // Get REAPER API functions
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  MediaItem_Take *(*GetActiveTake)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("GetActiveTake");
  MediaItem_Take *(*GetMediaItemTake)(MediaItem *, int) =
      (MediaItem_Take * (*)(MediaItem *, int))
          g_rec->GetFunc("GetMediaItemTake");
  int (*GetMediaItemNumTakes)(MediaItem *) =
      (int (*)(MediaItem *))g_rec->GetFunc("GetMediaItemNumTakes");
  PCM_source *(*GetMediaItemTake_Source)(MediaItem_Take *) =
      (PCM_source * (*)(MediaItem_Take *))
          g_rec->GetFunc("GetMediaItemTake_Source");
  MediaItem *(*CreateNewMIDIItemInProj)(MediaTrack *, double, double,
                                        const bool *) =
      (MediaItem * (*)(MediaTrack *, double, double, const bool *))
          g_rec->GetFunc("CreateNewMIDIItemInProj");
  bool (*SetMediaItemTake_Source)(MediaItem_Take *, PCM_source *) = (bool (*)(
      MediaItem_Take *, PCM_source *))g_rec->GetFunc("SetMediaItemTake_Source");
  bool (*MIDI_InsertNote)(MediaItem_Take *, bool, bool, double, double, int,
                          int, int, const bool *) =
      (bool (*)(MediaItem_Take *, bool, bool, double, double, int, int, int,
                const bool *))g_rec->GetFunc("MIDI_InsertNote");
  void (*MIDI_Sort)(MediaItem_Take *) =
      (void (*)(MediaItem_Take *))g_rec->GetFunc("MIDI_Sort");
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  double (*GetMediaItemPosition)(MediaItem *) =
      (double (*)(MediaItem *))g_rec->GetFunc("GetMediaItemPosition");

  // Check each function individually and log which ones are missing
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: AddMIDI: Checking REAPER API functions...\n");
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: GetTrack: %s\n",
             GetTrack ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: CountTrackMediaItems: %s\n",
             CountTrackMediaItems ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: GetTrackMediaItem: %s\n",
             GetTrackMediaItem ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: GetActiveTake: %s\n",
             GetActiveTake ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: GetMediaItemTake: %s\n",
             GetMediaItemTake ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: GetMediaItemNumTakes: %s\n",
             GetMediaItemNumTakes ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: GetMediaItemTake_Source: %s\n",
             GetMediaItemTake_Source ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: CreateNewMIDIItemInProj: %s\n",
             CreateNewMIDIItemInProj ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: SetMediaItemTake_Source: %s\n",
             SetMediaItemTake_Source ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: MIDI_InsertNote: %s\n",
             MIDI_InsertNote ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: MIDI_Sort: %s\n",
             MIDI_Sort ? "OK" : "MISSING");
    ShowConsoleMsg(log_msg);
  }

  if (!GetTrack || !CountTrackMediaItems || !GetTrackMediaItem ||
      !GetActiveTake || !GetMediaItemTake || !GetMediaItemNumTakes ||
      !GetMediaItemTake_Source || !CreateNewMIDIItemInProj ||
      !SetMediaItemTake_Source || !MIDI_InsertNote || !MIDI_Sort) {
    error_msg.Set("Required REAPER API functions not available");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: Required REAPER API functions not "
                     "available\n");
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: Missing functions listed above\n");
    }
    return false;
  }

  // Get the track
  MediaTrack *track = GetTrack(nullptr, track_index);
  if (!track) {
    error_msg.Set("Track not found");
    if (ShowConsoleMsg) {
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: AddMIDI ERROR: Track not found at index %d\n",
               track_index);
      ShowConsoleMsg(log_msg);
    }
    return false;
  }

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: Found track at index %d\n", track_index);
    ShowConsoleMsg(log_msg);
  }

  // Find the most recent clip on the track (or create one if none exists)
  MediaItem *item = nullptr;
  int num_items = CountTrackMediaItems(track);

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: Track has %d media items\n", num_items);
    ShowConsoleMsg(log_msg);
  }

  if (num_items > 0) {
    // Get the last clip
    item = GetTrackMediaItem(track, num_items - 1);
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Using existing media item\n");
  } else {
    // No clips exist, create one at bar 1, 4 bars long
    if (ShowConsoleMsg)
      ShowConsoleMsg(
          "MAGDA: AddMIDI: No clips exist, creating new clip at bar 1\n");
    if (!CreateClipAtBar(track_index, 1, 4, error_msg)) {
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI ERROR: Failed to create clip: %s\n",
                 error_msg.Get());
        ShowConsoleMsg(log_msg);
      }
      return false;
    }
    num_items = CountTrackMediaItems(track);
    if (num_items > 0) {
      item = GetTrackMediaItem(track, num_items - 1);
      if (ShowConsoleMsg)
        ShowConsoleMsg("MAGDA: AddMIDI: Successfully created new clip\n");
    }
  }

  if (!item) {
    error_msg.Set("Failed to get or create clip");
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: Failed to get or create clip\n");
    return false;
  }

  // Get or create a MIDI take
  MediaItem_Take *take = GetActiveTake(item);
  bool needs_midi_take = false;

  if (!take) {
    // No take exists, create one
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: No take exists, will create MIDI take\n");
    needs_midi_take = true;
  } else {
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Found existing take, checking if MIDI\n");
    // Check if the take is MIDI
    PCM_source *source = GetMediaItemTake_Source(take);
    if (!source) {
      if (ShowConsoleMsg)
        ShowConsoleMsg(
            "MAGDA: AddMIDI: Take has no source, will create MIDI take\n");
      needs_midi_take = true;
    } else {
      // Check if it's a MIDI source (we'll assume it's not if we can't verify)
      // For now, create a new MIDI take
      if (ShowConsoleMsg)
        ShowConsoleMsg("MAGDA: AddMIDI: Take has source but not MIDI, will "
                       "create MIDI take\n");
      needs_midi_take = true;
    }
  }

  if (needs_midi_take) {
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Creating MIDI item using "
                     "CreateNewMIDIItemInProj...\n");

    // Get item position and length to recreate it as MIDI
    double item_pos = GetMediaItemPosition ? GetMediaItemPosition(item) : 0.0;
    double item_len = 4.0; // Default to 4 seconds
    double (*GetMediaItemLength)(MediaItem *) =
        (double (*)(MediaItem *))g_rec->GetFunc("GetMediaItemLength");
    if (GetMediaItemLength) {
      double old_len = GetMediaItemLength(item);
      if (ShowConsoleMsg) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: Original clip length: %.2f seconds\n",
                 old_len);
        ShowConsoleMsg(log_msg);
      }
      item_len = old_len;
    }

    // Calculate required length from notes (find max end time in beats/quarter
    // notes)
    double max_end_beats = 0.0;
    int note_count = notes_array->m_array ? notes_array->m_array->GetSize() : 0;
    for (int i = 0; i < note_count; i++) {
      const wdl_json_element *note = notes_array->m_array->Get(i);
      if (note) {
        const wdl_json_element *start_el = note->get_item_by_name("start");
        const wdl_json_element *len_el = note->get_item_by_name("length");
        double note_start =
            (start_el && start_el->m_value) ? atof(start_el->m_value) : 0.0;
        double note_len =
            (len_el && len_el->m_value) ? atof(len_el->m_value) : 1.0;
        double note_end = note_start + note_len;
        if (note_end > max_end_beats)
          max_end_beats = note_end;
      }
    }

    // Ensure minimum length of 4 beats (1 bar)
    if (max_end_beats < 4.0) {
      max_end_beats = 4.0;
    }

    if (ShowConsoleMsg) {
      char log_msg[256];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: AddMIDI: Required length from notes: %.2f beats "
               "(quarter notes)\n",
               max_end_beats);
      ShowConsoleMsg(log_msg);
    }

    // Delete the old item
    bool (*DeleteTrackMediaItem)(MediaTrack *, MediaItem *) = (bool (*)(
        MediaTrack *, MediaItem *))g_rec->GetFunc("DeleteTrackMediaItem");
    if (DeleteTrackMediaItem) {
      DeleteTrackMediaItem(track, item);
      if (ShowConsoleMsg)
        ShowConsoleMsg("MAGDA: AddMIDI: Deleted old item\n");
    }

    // Get item position in quarter notes
    // Use TimeMap2_timeToQN to convert the original position from seconds to QN
    double (*TimeMap2_timeToQN)(ReaProject *, double) =
        (double (*)(ReaProject *, double))g_rec->GetFunc("TimeMap2_timeToQN");
    double item_pos_qn = 0.0;
    if (TimeMap2_timeToQN) {
      item_pos_qn = TimeMap2_timeToQN(nullptr, item_pos);
    }

    if (ShowConsoleMsg) {
      char log_msg[256];
      snprintf(
          log_msg, sizeof(log_msg),
          "MAGDA: AddMIDI: Creating MIDI item at %.2f QN, length %.2f QN\n",
          item_pos_qn, max_end_beats);
      ShowConsoleMsg(log_msg);
    }

    // Create new MIDI item using quarter notes (beats) - tempo independent!
    // qnInOptional = true means times are in quarter notes
    bool use_qn = true;
    item = CreateNewMIDIItemInProj(track, item_pos_qn,
                                   item_pos_qn + max_end_beats, &use_qn);
    if (!item) {
      error_msg.Set("Failed to create MIDI item");
      if (ShowConsoleMsg)
        ShowConsoleMsg("MAGDA: AddMIDI ERROR: Failed to create MIDI item\n");
      return false;
    }
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: MIDI item created successfully\n");

    // Verify and explicitly set the item length
    double (*GetMediaItemInfo_Value)(MediaItem *, const char *) = (double (*)(
        MediaItem *, const char *))g_rec->GetFunc("GetMediaItemInfo_Value");
    bool (*SetMediaItemInfo_Value)(MediaItem *, const char *, double) =
        (bool (*)(MediaItem *, const char *, double))g_rec->GetFunc(
            "SetMediaItemInfo_Value");
    double (*TimeMap2_QNToTime)(ReaProject *, double) =
        (double (*)(ReaProject *, double))g_rec->GetFunc("TimeMap2_QNToTime");

    if (GetMediaItemInfo_Value && SetMediaItemInfo_Value && TimeMap2_QNToTime) {
      double actual_pos = GetMediaItemInfo_Value(item, "D_POSITION");
      double actual_len = GetMediaItemInfo_Value(item, "D_LENGTH");
      if (ShowConsoleMsg) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: Item created - pos=%.4f sec, len=%.4f sec\n",
                 actual_pos, actual_len);
        ShowConsoleMsg(log_msg);
      }

      // Convert desired QN length to seconds using project tempo
      double desired_end_time =
          TimeMap2_QNToTime(nullptr, item_pos_qn + max_end_beats);
      double desired_start_time = TimeMap2_QNToTime(nullptr, item_pos_qn);
      double desired_len_seconds = desired_end_time - desired_start_time;

      if (ShowConsoleMsg) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: Desired length: %.4f sec (%.2f QN at current "
                 "tempo)\n",
                 desired_len_seconds, max_end_beats);
        ShowConsoleMsg(log_msg);
      }

      // If the item is shorter than desired, extend it
      if (actual_len < desired_len_seconds - 0.001) {
        SetMediaItemInfo_Value(item, "D_LENGTH", desired_len_seconds);
        if (ShowConsoleMsg) {
          char log_msg[256];
          snprintf(log_msg, sizeof(log_msg),
                   "MAGDA: AddMIDI: Extended item to %.4f sec\n",
                   desired_len_seconds);
          ShowConsoleMsg(log_msg);
        }
      }
    }

    // Get the active take from the new MIDI item
    take = GetActiveTake(item);
    if (!take) {
      error_msg.Set("Failed to get MIDI take from new item");
      if (ShowConsoleMsg)
        ShowConsoleMsg(
            "MAGDA: AddMIDI ERROR: Failed to get MIDI take from new item\n");
      return false;
    }
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Got active MIDI take\n");
  }

  if (!take) {
    error_msg.Set("Failed to get MIDI take");
    return false;
  }

  // Get REAPER's function to convert project QN to PPQ for this take
  double (*MIDI_GetPPQPosFromProjQN)(MediaItem_Take *, double) = (double (*)(
      MediaItem_Take *, double))g_rec->GetFunc("MIDI_GetPPQPosFromProjQN");

  if (!MIDI_GetPPQPosFromProjQN) {
    error_msg.Set("MIDI_GetPPQPosFromProjQN not available");
    if (ShowConsoleMsg)
      ShowConsoleMsg(
          "MAGDA: AddMIDI ERROR: MIDI_GetPPQPosFromProjQN not available\n");
    return false;
  }

  // Insert each note
  int notes_inserted = 0;
  bool noSort = true; // Don't sort until all notes are inserted

  // Count total notes first
  int total_notes = 0;
  int j = 0;
  wdl_json_element *temp_note = notes_array->enum_item(j);
  while (temp_note) {
    if (temp_note->is_object())
      total_notes++;
    j++;
    temp_note = notes_array->enum_item(j);
  }

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "MAGDA: AddMIDI: Processing %d notes\n",
             total_notes);
    ShowConsoleMsg(log_msg);
  }

  int i = 0;
  wdl_json_element *note_obj = notes_array->enum_item(i);
  while (note_obj) {
    if (!note_obj->is_object()) {
      i++;
      note_obj = notes_array->enum_item(i);
      continue;
    }

    // Parse note properties
    const char *pitch_str = note_obj->get_string_by_name("pitch", true);
    const char *velocity_str = note_obj->get_string_by_name("velocity", true);
    const char *start_str = note_obj->get_string_by_name("start", true);
    const char *length_str = note_obj->get_string_by_name("length", true);

    if (!pitch_str || !start_str || !length_str) {
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: Skipping invalid note at index %d (missing "
                 "fields)\n",
                 i);
        ShowConsoleMsg(log_msg);
      }
      i++;
      note_obj = notes_array->enum_item(i);
      continue; // Skip invalid notes
    }

    int pitch = atoi(pitch_str);
    int velocity = velocity_str ? atoi(velocity_str) : 100; // Default velocity
    double start_beats = atof(start_str);
    double length_beats = atof(length_str);

    // Convert beats (QN) to PPQ using REAPER's function - this handles tempo
    // correctly
    double start_ppq = MIDI_GetPPQPosFromProjQN(take, start_beats);
    double end_ppq = MIDI_GetPPQPosFromProjQN(take, start_beats + length_beats);

    if (ShowConsoleMsg) {
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: AddMIDI: Inserting note %d: pitch=%d, velocity=%d, "
               "start=%.2f QN (%.0f PPQ), end=%.2f QN (%.0f PPQ)\n",
               notes_inserted + 1, pitch, velocity, start_beats, start_ppq,
               start_beats + length_beats, end_ppq);
      ShowConsoleMsg(log_msg);
    }

    // Insert the note (channel 0, selected=false, muted=false)
    if (MIDI_InsertNote(take, false, false, start_ppq, end_ppq, 0, pitch,
                        velocity, &noSort)) {
      notes_inserted++;
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: Successfully inserted note %d\n",
                 notes_inserted);
        ShowConsoleMsg(log_msg);
      }
    } else {
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: AddMIDI: WARNING: MIDI_InsertNote returned false for "
                 "note %d (pitch=%d)\n",
                 notes_inserted + 1, pitch);
        ShowConsoleMsg(log_msg);
      }
    }

    i++;
    note_obj = notes_array->enum_item(i);
  }

  // Sort MIDI events after all insertions
  if (notes_inserted > 0) {
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Sorting MIDI events...\n");
    MIDI_Sort(take);
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: MIDI events sorted\n");
  }

  // Update UI
  if (UpdateArrange) {
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI: Updating arrange view...\n");
    UpdateArrange();
  }

  if (notes_inserted == 0) {
    error_msg.Set("No valid notes were inserted");
    if (ShowConsoleMsg)
      ShowConsoleMsg("MAGDA: AddMIDI ERROR: No valid notes were inserted\n");
    return false;
  }

  if (ShowConsoleMsg) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "MAGDA: AddMIDI: SUCCESS - Inserted %d notes out of %d total\n",
             notes_inserted, total_notes);
    ShowConsoleMsg(log_msg);
  }

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

// Normalize drum name from agent format to canonical format
// Agent uses: hat, hat_open -> Canonical uses: hi_hat, hi_hat_open
static std::string NormalizeDrumName(const char *drum_name) {
  if (!drum_name)
    return "";

  std::string name = drum_name;

  // Map agent drum names to canonical names
  if (name == "hat")
    return CanonicalDrums::HI_HAT;
  if (name == "hat_open")
    return CanonicalDrums::HI_HAT_OPEN;
  if (name == "hat_pedal")
    return CanonicalDrums::HI_HAT_PEDAL;

  // Already canonical or unknown - return as-is
  return name;
}

// Resolve drum name to MIDI note using drum mapping
// Falls back to General MIDI if no plugin mapping exists
int MagdaActions::ResolveDrumNote(const char *drum_name,
                                  const char *plugin_key) {
  std::string canonical_name = NormalizeDrumName(drum_name);

  // Default General MIDI drum notes
  static const std::map<std::string, int> gm_drums = {
      {CanonicalDrums::KICK, 36},         {CanonicalDrums::SNARE, 38},
      {CanonicalDrums::SNARE_RIM, 40},    {CanonicalDrums::SNARE_XSTICK, 37},
      {CanonicalDrums::HI_HAT, 42},       {CanonicalDrums::HI_HAT_OPEN, 46},
      {CanonicalDrums::HI_HAT_PEDAL, 44}, {CanonicalDrums::TOM_HIGH, 50},
      {CanonicalDrums::TOM_MID, 47},      {CanonicalDrums::TOM_LOW, 45},
      {CanonicalDrums::CRASH, 49},        {CanonicalDrums::RIDE, 51},
      {CanonicalDrums::RIDE_BELL, 53},
  };

  // Try to use plugin-specific mapping first
  if (plugin_key && plugin_key[0] && g_drumMappingManager) {
    const DrumMapping *mapping =
        g_drumMappingManager->GetMappingForPlugin(plugin_key);
    if (mapping) {
      int note = mapping->GetNote(canonical_name);
      if (note >= 0) {
        return note;
      }
    }
  }

  // Fall back to General MIDI
  auto it = gm_drums.find(canonical_name);
  if (it != gm_drums.end()) {
    return it->second;
  }

  // Unknown drum - return -1
  return -1;
}

// Add drum pattern to track - converts grid notation to MIDI notes
bool MagdaActions::AddDrumPattern(int track_index, const char *drum_name,
                                  const char *grid, int velocity,
                                  const char *plugin_key,
                                  WDL_FastString &error_msg) {
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  if (!drum_name || !grid) {
    error_msg.Set("drum_pattern: missing drum name or grid");
    return false;
  }

  // Resolve drum name to MIDI note
  int midi_note = ResolveDrumNote(drum_name, plugin_key);
  if (midi_note < 0) {
    error_msg.Set("drum_pattern: unknown drum name '");
    error_msg.Append(drum_name);
    error_msg.Append("'");
    return false;
  }

  // Convert velocity from 0-127 range, clamping if needed
  if (velocity < 0)
    velocity = 0;
  if (velocity > 127)
    velocity = 127;

  // Parse grid and calculate note positions
  // Grid: 16 chars = 1 bar (each char is 1/16 note)
  // "x" = hit, "X" = accent, "o" = ghost, "-" = rest
  size_t grid_len = strlen(grid);
  if (grid_len == 0) {
    error_msg.Set("drum_pattern: empty grid");
    return false;
  }

  // Build notes array for AddMIDI
  // Each note: {"pitch":N, "velocity":V, "start":beats, "length":beats}
  WDL_FastString notes_json;
  notes_json.Append("[");

  int note_count = 0;
  double sixteenth_note = 0.25; // 1/16 note = 0.25 beats (quarter notes)

  for (size_t i = 0; i < grid_len; i++) {
    char c = grid[i];
    int note_velocity = 0;

    switch (c) {
    case 'x': // Normal hit
      note_velocity = velocity;
      break;
    case 'X': // Accent
      note_velocity = 127;
      break;
    case 'o': // Ghost note
      note_velocity = 60;
      break;
    case '-': // Rest
    default:
      continue;
    }

    // Calculate position in beats (quarter notes)
    double start_beats = i * sixteenth_note;
    double length_beats = sixteenth_note; // 1/16 note duration

    if (note_count > 0) {
      notes_json.Append(",");
    }

    char note_json[256];
    snprintf(note_json, sizeof(note_json),
             "{\"pitch\":%d,\"velocity\":%d,\"start\":%.4f,\"length\":%.4f}",
             midi_note, note_velocity, start_beats, length_beats);
    notes_json.Append(note_json);
    note_count++;
  }

  notes_json.Append("]");

  if (note_count == 0) {
    // No notes to add - grid was all rests, which is fine
    return true;
  }

  // Log the drum pattern
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: drum_pattern: drum=%s, note=%d, grid=%s, %d hits\n",
               drum_name, midi_note, grid, note_count);
      ShowConsoleMsg(log_msg);
    }
  }

  // Parse the notes JSON and call AddMIDI
  wdl_json_parser parser;
  wdl_json_element *notes_array =
      parser.parse(notes_json.Get(), (int)notes_json.GetLength());

  if (parser.m_err || !notes_array) {
    error_msg.Set("drum_pattern: internal error parsing notes");
    return false;
  }

  return AddMIDI(track_index, notes_array, error_msg);
}
