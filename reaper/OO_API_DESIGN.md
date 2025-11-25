# Object-Oriented REAPER API Design

## Architecture Overview

```
LLM Structured Output (JSON)
  ↓
Action Parser/Translator
  ↓
Object-Oriented API Calls
  ↓
REAPER API (wrapped)
```

## Example Flow

### LLM Output:
```json
{
  "action": "create_track",
  "name": "Bass",
  "instrument": "VST3:Serum (Xfer Records)"
}
```

### Translation:
```cpp
// In ExecuteAction():
Track* track = Track::create(
  name: "Bass",
  instrument: "VST3:Serum (Xfer Records)"
);
```

### Implementation:
```cpp
Track* Track::create(const char* name, const char* instrument) {
  // Handle all REAPER API calls internally
  MediaTrack* reaper_track = ReaperAPI::InsertTrack(...);
  if (name) ReaperAPI::SetTrackName(reaper_track, name);
  if (instrument) ReaperAPI::AddTrackFX(reaper_track, instrument);
  return new Track(reaper_track);
}
```

---

## Core Classes

### 1. Track

```cpp
class Track {
private:
  MediaTrack* m_reaper_track;
  int m_index;

public:
  // Factory method
  static Track* create(int index = -1, const char* name = nullptr, const char* instrument = nullptr);

  // Getters
  MediaTrack* getReaperTrack() const { return m_reaper_track; }
  int getIndex() const { return m_index; }
  const char* getName() const;

  // Operations
  bool setName(const char* name);
  bool addInstrument(const char* fxname);
  bool addFX(const char* fxname);
  MediaItem* addClip(double position, double length);
  MediaItem* addClipAtBar(int bar, int length_bars = 4);

  bool setVolume(double volume_db);
  bool setPan(double pan);
  bool setMute(bool mute);
  bool setSolo(bool solo);

  // Static helpers
  static Track* findByIndex(int index);
  static Track* findByName(const char* name);
  static int getCount();
};
```

### 2. MediaItem (Clip)

```cpp
class MediaItem {
private:
  MediaItem* m_reaper_item;
  MediaTrack* m_track;

public:
  // Factory method
  static MediaItem* create(Track* track, double position, double length);
  static MediaItem* createAtBar(Track* track, int bar, int length_bars = 4);

  // Operations
  bool setPosition(double position);
  bool setLength(double length);
  bool setPositionAtBar(int bar);
  bool setLengthInBars(int bars);

  // Getters
  double getPosition() const;
  double getLength() const;
  Track* getTrack() const;
};
```

### 3. Project (for time conversions, etc.)

```cpp
class Project {
public:
  // Time conversion
  static double barToTime(int bar);
  static int timeToBar(double time);
  static double barsToTime(int bars);

  // Project info
  static const char* getName();
  static double getLength();
  static double getTempo();
  static int getTimeSignatureNumerator();
  static int getTimeSignatureDenominator();

  // Updates
  static void updateArrange();
};
```

---

## Action Translation

### Action Handler → OO Call Mapping

```cpp
bool MagdaActions::ExecuteAction(const wdl_json_element *action, ...) {
  const char *action_type = action->get_string_by_name("action");

  if (strcmp(action_type, "create_track") == 0) {
    // Parse JSON
    const char *name = action->get_string_by_name("name");
    const char *instrument = action->get_string_by_name("instrument");
    const char *index_str = action->get_string_by_name("index");
    int index = index_str ? atoi(index_str) : -1;

    // Translate to OO call
    Track* track = Track::create(index, name, instrument);

    if (track) {
      result.Append("{\"action\":\"create_track\",\"success\":true,\"index\":");
      result.Append(track->getIndex());
      result.Append("}");
      return true;
    }
    return false;
  }

  else if (strcmp(action_type, "create_clip") == 0) {
    // Parse JSON
    const char *track_str = action->get_string_by_name("track");
    const char *position_str = action->get_string_by_name("position");
    const char *bar_str = action->get_string_by_name("bar");
    const char *length_str = action->get_string_by_name("length");
    const char *length_bars_str = action->get_string_by_name("length_bars");

    // Get track
    int track_index = atoi(track_str);
    Track* track = Track::findByIndex(track_index);
    if (!track) {
      error_msg.Set("Track not found");
      return false;
    }

    // Translate to OO call
    MediaItem* item = nullptr;
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
    return false;
  }

  // ... more actions
}
```

---

## Implementation Structure

```
magda/reaper/
├── include/
│   ├── reaper_api_wrapper.h      // Low-level REAPER API wrapper
│   ├── track.h                    // Track class
│   ├── media_item.h               // MediaItem class
│   ├── project.h                  // Project utilities
│   └── magda_actions.h            // Action execution (uses OO API)
├── src/
│   ├── reaper_api_wrapper.cpp    // REAPER API function pointer caching
│   ├── track.cpp                  // Track implementation
│   ├── media_item.cpp             // MediaItem implementation
│   ├── project.cpp                // Project utilities
│   └── magda_actions.cpp          // Action → OO translation
```

---

## Example: Complete Flow

### User Request:
"Add a track with Serum and create a clip at bar 17"

### LLM Output:
```json
{
  "actions": [
    {
      "action": "create_track",
      "name": "Bass",
      "instrument": "VST3:Serum (Xfer Records)"
    },
    {
      "action": "create_clip",
      "track": "0",
      "bar": 17,
      "length_bars": 4
    }
  ]
}
```

### Code Execution:
```cpp
// Action 1: create_track
Track* track = Track::create(-1, "Bass", "VST3:Serum (Xfer Records)");
// Internally calls:
//   - ReaperAPI::InsertTrack()
//   - ReaperAPI::SetTrackName()
//   - ReaperAPI::AddTrackFX()

// Action 2: create_clip
Track* track = Track::findByIndex(0);  // Get track from action 1
MediaItem* item = MediaItem::createAtBar(track, 17, 4);
// Internally calls:
//   - Project::barToTime(17) to convert bar to time
//   - ReaperAPI::AddMediaItem()
//   - ReaperAPI::SetMediaItemPosition()
//   - ReaperAPI::SetMediaItemLength()
```

---

## Benefits

1. **Clean Translation**: LLM output maps directly to intuitive OO calls
2. **Encapsulation**: REAPER API complexity hidden in objects
3. **Reusability**: Track/MediaItem objects can be used elsewhere
4. **Testability**: Can mock objects for testing
5. **Extensibility**: Easy to add new methods (e.g., `track->addAutomation()`)
6. **Type Safety**: Compile-time checking vs string-based API

---

## Next Steps

1. Create `reaper_api_wrapper.h/cpp` (low-level function pointer caching)
2. Create `track.h/cpp` (Track class)
3. Create `media_item.h/cpp` (MediaItem class)
4. Create `project.h/cpp` (Project utilities)
5. Refactor `magda_actions.cpp` to use OO API
6. Update action format documentation
