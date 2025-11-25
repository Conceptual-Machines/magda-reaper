#pragma once

#include "reaper_api_wrapper.h"
#include "reaper_plugin.h"
#include "track.h"

// High-level MediaItem (clip) object
class MediaItem {
private:
  MediaItem *m_reaper_item;
  Track *m_track;

public:
  // Factory methods
  static MediaItem *create(Track *track, double position, double length);
  static MediaItem *createAtBar(Track *track, int bar, int length_bars = 4);

  // Operations
  bool setPosition(double position);
  bool setLength(double length);
  bool setPositionAtBar(int bar);
  bool setLengthInBars(int bars);

  // Getters
  double getPosition() const;
  double getLength() const;
  Track *getTrack() const { return m_track; }
  MediaItem *getReaperItem() const { return m_reaper_item; }

private:
  // Private constructor - use create() factory methods
  MediaItem(MediaItem *item, Track *track);
};
