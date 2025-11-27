#pragma once

#include "reaper_plugin.h"

// Low-level wrapper for REAPER API functions
// Caches function pointers for performance
class ReaperAPI {
public:
  // Initialize API (call once at plugin load)
  static bool Initialize(reaper_plugin_info_t *rec);
  static bool IsAvailable();

  // Track operations
  static MediaTrack *InsertTrack(int index, int flags = 1);
  static MediaTrack *GetTrack(int index);
  static int GetNumTracks();
  static bool SetTrackName(MediaTrack *track, const char *name);
  static bool GetTrackName(MediaTrack *track, char *buf, int buf_size);

  // Media item operations
  static MediaItem *AddMediaItem(MediaTrack *track);
  static bool SetMediaItemPosition(MediaItem *item, double position);
  static bool SetMediaItemLength(MediaItem *item, double length);
  static double GetMediaItemPosition(MediaItem *item);
  static double GetMediaItemLength(MediaItem *item);

  // Track properties
  static bool SetTrackVolume(MediaTrack *track, double volume_db);
  static bool SetTrackPan(MediaTrack *track, double pan);
  static bool SetTrackMute(MediaTrack *track, bool mute);
  static bool SetTrackSolo(MediaTrack *track, bool solo);
  static bool GetTrackVolume(MediaTrack *track, double *volume_db);
  static bool GetTrackPan(MediaTrack *track, double *pan);
  static bool GetTrackMute(MediaTrack *track, bool *mute);
  static bool GetTrackSolo(MediaTrack *track, bool *solo);

  // FX operations
  static int AddTrackFX(MediaTrack *track, const char *fxname,
                        bool recFX = false);

  // Time conversion
  static double BarToTime(int bar);
  static int TimeToBar(double time);
  static double BarsToTime(int bars);

  // Project operations
  static void UpdateArrange();

private:
  static reaper_plugin_info_t *s_rec;
  static bool s_initialized;

  // Cached function pointers
  static void (*s_InsertTrackInProject)(ReaProject *, int, int);
  static MediaTrack *(*s_GetTrack)(ReaProject *, int);
  static int (*s_GetNumTracks)(ReaProject *);
  static void *(*s_GetSetMediaTrackInfo)(INT_PTR, const char *, void *, bool *);

  static MediaItem *(*s_AddMediaItemToTrack)(MediaTrack *);
  static bool (*s_SetMediaItemPosition)(MediaItem *, double, bool);
  static bool (*s_SetMediaItemLength)(MediaItem *, double, bool);
  static double (*s_GetMediaItemPosition)(MediaItem *);
  static double (*s_GetMediaItemLength)(MediaItem *);

  static bool (*s_GetTrackUIVolPan)(MediaTrack *, double *, double *);
  static bool (*s_SetTrackUIVolPan)(MediaTrack *, double, double);
  static bool (*s_GetTrackUIMute)(MediaTrack *, bool *);
  static bool (*s_SetTrackUIMute)(MediaTrack *, bool);
  static bool (*s_GetTrackUISolo)(MediaTrack *, bool *);
  static bool (*s_SetTrackUISolo)(MediaTrack *, bool);

  static int (*s_TrackFX_AddByName)(MediaTrack *, const char *, bool, int);

  static double (*s_TimeMap_GetMeasureInfo)(ReaProject *, int, double *,
                                            double *, int *, int *, double *);
  static double (*s_TimeMap2_QNToTime)(ReaProject *, double);
  static double (*s_TimeMap2_timeToQN)(ReaProject *, double);

  static void (*s_UpdateArrange)();
};
