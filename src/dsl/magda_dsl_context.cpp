#include "magda_dsl_context.h"
#include "reaper_plugin.h"
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// ============================================================================
// Singleton
// ============================================================================

MagdaDSLContext &MagdaDSLContext::Get() {
  static MagdaDSLContext instance;
  return instance;
}

MagdaDSLContext::MagdaDSLContext() {
  Clear();
}

// ============================================================================
// Clear
// ============================================================================

void MagdaDSLContext::Clear() {
  m_createdTrackIndex = -1;
  m_createdTrackName.clear();
  m_createdClipTrackIndex = -1;
  m_createdClipItemIndex = -1;
}

// ============================================================================
// Track Context
// ============================================================================

void MagdaDSLContext::SetCreatedTrack(int index, const char *name) {
  m_createdTrackIndex = index;
  m_createdTrackName = name ? name : "";

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA Context: Track created - index=%d name='%s'\n", index,
               m_createdTrackName.c_str());
      ShowConsoleMsg(msg);
    }
  }
}

// ============================================================================
// Clip Context
// ============================================================================

void MagdaDSLContext::SetCreatedClip(int trackIndex, int itemIndex) {
  m_createdClipTrackIndex = trackIndex;
  m_createdClipItemIndex = itemIndex;

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA Context: Clip created - track=%d item=%d\n", trackIndex,
               itemIndex);
      ShowConsoleMsg(msg);
    }
  }
}

// ============================================================================
// Smart Resolution
// ============================================================================

int MagdaDSLContext::FindTrackByName(const char *name) {
  if (!name || !*name || !g_rec)
    return -1;

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  bool (*GetTrackName)(MediaTrack *, char *, int) =
      (bool (*)(MediaTrack *, char *, int))g_rec->GetFunc("GetTrackName");

  if (!GetNumTracks || !GetTrack || !GetTrackName)
    return -1;

  int numTracks = GetNumTracks();
  for (int i = 0; i < numTracks; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (!track)
      continue;

    char trackName[256] = {0};
    GetTrackName(track, trackName, sizeof(trackName));

    // Case-insensitive comparison
    if (strcasecmp(trackName, name) == 0) {
      return i;
    }
  }

  return -1;
}

int MagdaDSLContext::ResolveTargetTrack(const char *trackName) {
  // 1. If specific name given, try to find it
  if (trackName && *trackName) {
    int found = FindTrackByName(trackName);
    if (found >= 0) {
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg), "MAGDA Context: Resolved track '%s' to index %d\n", trackName,
                   found);
          ShowConsoleMsg(msg);
        }
      }
      return found;
    }
  }

  // 2. If a track was created this session, use it
  if (m_createdTrackIndex >= 0) {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA Context: Using created track %d ('%s')\n",
                 m_createdTrackIndex, m_createdTrackName.c_str());
        ShowConsoleMsg(msg);
      }
    }
    return m_createdTrackIndex;
  }

  // 3. Use selected track
  if (!g_rec)
    return 0;

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  int (*IsTrackSelected)(MediaTrack *) = (int (*)(MediaTrack *))g_rec->GetFunc("IsTrackSelected");

  if (!GetNumTracks || !GetTrack || !IsTrackSelected)
    return 0;

  int numTracks = GetNumTracks();
  for (int i = 0; i < numTracks; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track && IsTrackSelected(track)) {
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg), "MAGDA Context: Using selected track %d\n", i);
          ShowConsoleMsg(msg);
        }
      }
      return i;
    }
  }

  // Fallback to track 0
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA Context: No track context, using track 0\n");
    }
  }
  return 0;
}
