#pragma once

#include "reaper_api_wrapper.h"
#include "reaper_plugin.h"

// High-level Track object
// Translates LLM actions to REAPER operations
class Track {
private:
  MediaTrack *m_reaper_track;
  int m_index;

public:
  // Factory method - creates track and returns object
  // index: -1 to append at end, otherwise insert at index
  static Track *create(int index = -1, const char *name = nullptr,
                       const char *instrument = nullptr);

  // Getters
  MediaTrack *getReaperTrack() const { return m_reaper_track; }
  int getIndex() const { return m_index; }
  bool getName(char *buf, int buf_size) const;

  // Operations (return Track* for chaining, or MediaItem* for clip operations)
  Track *setName(const char *name);
  Track *addInstrument(const char *fxname);
  Track *addFX(const char *fxname);
  MediaItem *addClip(double position, double length);
  MediaItem *addClipAtBar(int bar, int length_bars = 4);

  Track *setVolume(double volume_db);
  Track *setPan(double pan);
  Track *setMute(bool mute);
  Track *setSolo(bool solo);

  // Static helpers
  static Track *findByIndex(int index);
  static Track *findByName(const char *name);
  static int getCount();

private:
  // Private constructor - use create() factory method
  Track(MediaTrack *track, int index);
};
