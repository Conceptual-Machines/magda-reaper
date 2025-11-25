// Example: How ExecuteAction() translates LLM JSON to OO calls

#include "magda_actions.h"
#include "track.h"
#include "media_item.h"
#include "project.h"

bool MagdaActions::ExecuteAction(const wdl_json_element *action,
                                  WDL_FastString &result,
                                  WDL_FastString &error_msg) {
  const char *action_type = action->get_string_by_name("action");

  // ============================================================
  // Example 1: create_track
  // LLM Output: {"action": "create_track", "name": "Bass", "instrument": "VST3:Serum"}
  // ============================================================
  if (strcmp(action_type, "create_track") == 0) {
    const char *name = action->get_string_by_name("name");
    const char *instrument = action->get_string_by_name("instrument");
    const char *index_str = action->get_string_by_name("index");
    int index = index_str ? atoi(index_str) : -1;

    // Translate to OO call
    Track *track = Track::create(index, name, instrument);

    if (track) {
      result.Append("{\"action\":\"create_track\",\"success\":true,\"index\":");
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", track->getIndex());
      result.Append(buf);
      result.Append("}");
      return true;
    }
    error_msg.Set("Failed to create track");
    return false;
  }

  // ============================================================
  // Example 2: create_clip (with bar)
  // LLM Output: {"action": "create_clip", "track": "0", "bar": 17, "length_bars": 4}
  // ============================================================
  else if (strcmp(action_type, "create_clip") == 0) {
    const char *track_str = action->get_string_by_name("track");
    const char *position_str = action->get_string_by_name("position");
    const char *bar_str = action->get_string_by_name("bar");
    const char *length_str = action->get_string_by_name("length");
    const char *length_bars_str = action->get_string_by_name("length_bars");

    // Get track
    int track_index = track_str ? atoi(track_str) : -1;
    Track *track = Track::findByIndex(track_index);
    if (!track) {
      error_msg.Set("Track not found");
      return false;
    }

    // Translate to OO call - supports both bar and time
    MediaItem *item = nullptr;
    if (bar_str) {
      // Use bar-based creation
      int bar = atoi(bar_str);
      int length_bars = length_bars_str ? atoi(length_bars_str) : 4;
      item = MediaItem::createAtBar(track, bar, length_bars);
    } else if (position_str) {
      // Use time-based creation
      double position = atof(position_str);
      double length = length_str ? atof(length_str) : 4.0;
      item = MediaItem::create(track, position, length);
    } else {
      error_msg.Set("Must provide either 'bar' or 'position'");
      return false;
    }

    if (item) {
      result.Append("{\"action\":\"create_clip\",\"success\":true}");
      return true;
    }
    error_msg.Set("Failed to create clip");
    return false;
  }

  // ============================================================
  // Example 3: set_track_volume
  // LLM Output: {"action": "set_track_volume", "track": "0", "volume_db": "-3.0"}
  // ============================================================
  else if (strcmp(action_type, "set_track_volume") == 0) {
    const char *track_str = action->get_string_by_name("track");
    const char *volume_str = action->get_string_by_name("volume_db");

    if (!track_str || !volume_str) {
      error_msg.Set("Missing 'track' or 'volume_db' field");
      return false;
    }

    int track_index = atoi(track_str);
    double volume_db = atof(volume_str);

    // Translate to OO call
    Track *track = Track::findByIndex(track_index);
    if (!track) {
      error_msg.Set("Track not found");
      return false;
    }

    if (track->setVolume(volume_db)) {
      result.Append("{\"action\":\"set_track_volume\",\"success\":true}");
      return true;
    }
    error_msg.Set("Failed to set track volume");
    return false;
  }

  // ... more actions follow same pattern
}
