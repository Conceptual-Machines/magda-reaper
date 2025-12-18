#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// ImGui-based mix analysis dialog - collects track type and optional query
class MagdaImGuiMixAnalysisDialog {
public:
  struct Result {
    bool cancelled;
    WDL_FastString trackType; // e.g., "drums", "bass", "guitar", etc.
    WDL_FastString userQuery; // Optional user query/prompt
  };

  MagdaImGuiMixAnalysisDialog();
  ~MagdaImGuiMixAnalysisDialog();

  // Initialize ReaImGui function pointers
  bool Initialize(reaper_plugin_info_t *rec);

  // Check if ReaImGui is available
  bool IsAvailable() const { return m_available; }

  // Show dialog
  void Show();

  // Check if dialog is currently visible
  bool IsVisible() const { return m_visible; }

  // Check if dialog has been completed (user clicked OK or Cancel)
  bool IsCompleted() const { return m_completed; }

  // Get the result (only valid after IsCompleted() returns true)
  const Result &GetResult() const { return m_dialogResult; }

  // Reset dialog state (call after processing result)
  void Reset();

  // Main render loop - call from timer
  void Render();

private:
  // ReaImGui function pointers
  void *(*m_ImGui_CreateContext)(const char *label,
                                 int *config_flagsInOptional);
  bool (*m_ImGui_Begin)(void *ctx, const char *name, bool *p_openInOutOptional,
                        int *flagsInOptional);
  void (*m_ImGui_End)(void *ctx);
  void (*m_ImGui_SetNextWindowSize)(void *ctx, double size_w, double size_h,
                                    int *condInOptional);
  void (*m_ImGui_Text)(void *ctx, const char *text);
  bool (*m_ImGui_Combo)(void *ctx, const char *label, int *current_item,
                        const char *const *items, int items_count);
  bool (*m_ImGui_InputText)(void *ctx, const char *label, char *buf, int buf_sz,
                            int *flagsInOptional, void *callbackInOptional);
  bool (*m_ImGui_Button)(void *ctx, const char *label, double *size_wInOptional,
                         double *size_hInOptional);
  void (*m_ImGui_SameLine)(void *ctx, double *offset_from_start_xInOptional,
                           double *spacingInOptional);
  void (*m_ImGui_Separator)(void *ctx);

  // State
  bool m_available = false;
  bool m_visible = false;
  void *m_ctx = nullptr;

  // Dialog state
  Result m_dialogResult;
  bool m_completed = false;
  int m_selectedTrackType = 0; // Index into track types array
  char m_userQueryBuffer[512] = {0};

  // Track type options
  static const char *s_trackTypes[];
  static const int s_trackTypeCount;
};
