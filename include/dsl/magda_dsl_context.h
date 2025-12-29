#pragma once

#include <string>

// ============================================================================
// DSL Execution Context
// ============================================================================
// Tracks entities created during a DSL session so subsequent commands
// can reference them implicitly or by name.
//
// Example flow:
//   1. User: "Create a track with drums"
//   2. DAW creates track index 3, name "Drums" → stored in context
//   3. Arranger/Drummer sees context → uses track 3
//   4. Context cleared after command completes
//
// This enables:
//   - "Add track with arpeggio" → notes go to new track
//   - "Add arpeggio" → notes go to selected track
//   - "Add clip to Drums track" → finds track by name

class MagdaDSLContext {
public:
  // Singleton access
  static MagdaDSLContext &Get();

  // Clear all context (call at start/end of DSL processing)
  void Clear();

  // ==================== Track Context ====================

  // Set when a track is created
  void SetCreatedTrack(int index, const char *name = nullptr);

  // Get last created track index (-1 if none)
  int GetCreatedTrackIndex() const { return m_createdTrackIndex; }

  // Get last created track name (empty if none)
  const std::string &GetCreatedTrackName() const { return m_createdTrackName; }

  // Check if a track was created in this session
  bool HasCreatedTrack() const { return m_createdTrackIndex >= 0; }

  // ==================== Clip/Item Context ====================

  // Set when a clip/item is created
  void SetCreatedClip(int trackIndex, int itemIndex);

  // Get track index of last created clip (-1 if none)
  int GetCreatedClipTrackIndex() const { return m_createdClipTrackIndex; }

  // Get item index of last created clip (-1 if none)
  int GetCreatedClipItemIndex() const { return m_createdClipItemIndex; }

  // Check if a clip was created in this session
  bool HasCreatedClip() const { return m_createdClipItemIndex >= 0; }

  // ==================== Smart Resolution ====================

  // Get the best track index for adding content:
  // 1. If track was created this session → use it
  // 2. If a specific name is given → find by name
  // 3. Otherwise → use selected track
  int ResolveTargetTrack(const char *trackName = nullptr);

  // Find track by name (returns -1 if not found)
  int FindTrackByName(const char *name);

private:
  MagdaDSLContext();

  // Track context
  int m_createdTrackIndex = -1;
  std::string m_createdTrackName;

  // Clip context
  int m_createdClipTrackIndex = -1;
  int m_createdClipItemIndex = -1;
};
