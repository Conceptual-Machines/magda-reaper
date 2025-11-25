# REAPER API Wrapper Design

## Current Architecture (Direct API Calls)

**Current Flow:**
```
Structured Output (JSON)
  → ExecuteAction() [parses JSON]
    → CreateTrack() [direct REAPER API calls]
      → g_rec->GetFunc("InsertTrackInProject")
      → g_rec->GetFunc("GetTrack")
      → g_rec->GetFunc("GetSetMediaTrackInfo")
```

**Problems:**
1. **Repetitive function pointer lookups** - Every function does `g_rec->GetFunc()` calls
2. **No abstraction** - Action handlers directly deal with REAPER API quirks
3. **Error handling scattered** - Each function checks if API is available
4. **Hard to test** - Can't mock REAPER API calls
5. **Code duplication** - Same patterns repeated (get track, check null, etc.)

---

## Proposed Architecture (Wrapper Layer)

**Proposed Flow:**
```
Structured Output (JSON)
  → ExecuteAction() [parses JSON]
    → CreateTrack() [calls wrapper]
      → ReaperAPI::InsertTrack()
      → ReaperAPI::GetTrack()
      → ReaperAPI::SetTrackName()
```

---

## Option 1: Thin Wrapper (Recommended)

Create a `ReaperAPI` class that:
- Caches function pointers (lookup once, reuse)
- Provides simple, clean function signatures
- Handles common error cases
- Still exposes REAPER types (MediaTrack*, etc.)

### Example:

```cpp
// reaper_api_wrapper.h
class ReaperAPI {
public:
  static bool Initialize(reaper_plugin_info_t *rec);
  static bool IsAvailable();

  // Track operations
  static MediaTrack* InsertTrack(int index, int flags = 1);
  static MediaTrack* GetTrack(int index);
  static bool SetTrackName(MediaTrack* track, const char* name);
  static int GetNumTracks();

  // Media item operations
  static MediaItem* AddMediaItem(MediaTrack* track);
  static bool SetMediaItemPosition(MediaItem* item, double position);
  static bool SetMediaItemLength(MediaItem* item, double length);

  // FX operations
  static int AddTrackFX(MediaTrack* track, const char* fxname);

  // Time operations
  static double BarToTime(int bar);
  static int TimeToBar(double time);

  // Utility
  static void UpdateArrange();
};

// Usage in CreateTrack():
bool MagdaActions::CreateTrack(int index, const char *name, ...) {
  if (!ReaperAPI::IsAvailable()) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  MediaTrack* track = ReaperAPI::InsertTrack(index);
  if (!track) {
    error_msg.Set("Failed to create track");
    return false;
  }

  if (name && name[0]) {
    ReaperAPI::SetTrackName(track, name);
  }

  return true;
}
```

**Pros:**
- Clean, readable code
- Function pointers cached (performance)
- Easy to add logging/debugging
- Can add convenience helpers (bar→time conversion)
- Still uses REAPER types (no abstraction overhead)

**Cons:**
- Extra layer of indirection
- Need to maintain wrapper

---

## Option 2: High-Level Abstractions

Create domain-specific classes that hide REAPER details:

```cpp
class Track {
public:
  static Track* Create(const char* name, int index = -1);
  bool SetName(const char* name);
  bool AddInstrument(const char* fxname);
  MediaItem* AddClip(double position, double length);
  // ...
};

// Usage:
Track* track = Track::Create("Drums");
track->AddInstrument("VST3:ReaSynth");
track->AddClip(64.0, 16.0);
```

**Pros:**
- Very clean, intuitive API
- Hides all REAPER complexity
- Easy to understand for backend developers

**Cons:**
- More abstraction = more code to maintain
- May hide important REAPER features
- Need to decide what to expose/hide

---

## Option 3: Hybrid Approach

Keep direct API calls for simple operations, add wrappers for complex/common patterns:

```cpp
// Direct calls for simple things:
void (*InsertTrack)(...) = g_rec->GetFunc("InsertTrackInProject");
InsertTrack(nullptr, index, 1);

// Wrappers for complex/common patterns:
ReaperAPI::BarToTime(17);  // Helper function
ReaperAPI::AddTrackWithInstrument("Drums", "VST3:ReaSynth");  // Convenience
```

**Pros:**
- Flexible
- Only wrap what's needed

**Cons:**
- Inconsistent style
- Hard to know when to use what

---

## Recommendation: Option 1 (Thin Wrapper)

### Implementation Plan

1. **Create `reaper_api_wrapper.h/cpp`**
   - Cache all function pointers in static variables
   - Initialize once at plugin load
   - Provide clean function signatures

2. **Refactor existing action handlers**
   - Replace direct `g_rec->GetFunc()` calls with wrapper calls
   - Simplify error handling

3. **Add convenience helpers**
   - `BarToTime()`, `TimeToBar()`
   - `GetTrackByName()`
   - Common validation patterns

### Example Wrapper Implementation

```cpp
// reaper_api_wrapper.h
class ReaperAPI {
private:
  static reaper_plugin_info_t* s_rec;
  static bool s_initialized;

  // Cached function pointers
  static void (*s_InsertTrackInProject)(ReaProject*, int, int);
  static MediaTrack* (*s_GetTrack)(ReaProject*, int);
  static void* (*s_GetSetMediaTrackInfo)(INT_PTR, const char*, void*, bool*);
  // ... more cached pointers

public:
  static bool Initialize(reaper_plugin_info_t* rec);

  // Track API
  static MediaTrack* InsertTrack(int index, int flags = 1);
  static MediaTrack* GetTrack(int index);
  static bool SetTrackName(MediaTrack* track, const char* name);
  static int GetNumTracks();

  // Media item API
  static MediaItem* AddMediaItem(MediaTrack* track);
  static bool SetMediaItemPosition(MediaItem* item, double position);
  static bool SetMediaItemLength(MediaItem* item, double length);

  // FX API
  static int AddTrackFX(MediaTrack* track, const char* fxname, bool recFX = false);

  // Time conversion helpers
  static double BarToTime(int bar);
  static int TimeToBar(double time);

  // Utility
  static void UpdateArrange();
};
```

---

## Mapping Strategy

### Structured Output → Action Handler → Wrapper → REAPER API

```
JSON: {"action": "create_track", "name": "Drums"}
  ↓
ExecuteAction() parses JSON
  ↓
CreateTrack(index, name) called
  ↓
ReaperAPI::InsertTrack(index)
  ↓
ReaperAPI::SetTrackName(track, name)
  ↓
REAPER API functions (cached pointers)
```

### Benefits:
- **Clear separation**: JSON parsing → Business logic → API calls
- **Testable**: Can mock ReaperAPI for testing
- **Maintainable**: Changes to REAPER API only affect wrapper
- **Readable**: Action handlers are clean and focused

---

## Decision Needed

**Do we create the wrapper layer?**

**Yes (Recommended):**
- Cleaner code
- Better maintainability
- Easier to add features (bar conversion, etc.)
- Performance (cached function pointers)

**No (Current approach):**
- Less code to maintain
- Direct control
- No abstraction overhead
