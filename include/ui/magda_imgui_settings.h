#pragma once

#include "magda_state.h"
#include "reaper_plugin.h"
#include <string>

// StateFilterMode and StateFilterPreferences are defined in magda_state.h

class MagdaImGuiSettings {
public:
  MagdaImGuiSettings();
  ~MagdaImGuiSettings();

  // Initialize ReaImGui function pointers
  bool Initialize(reaper_plugin_info_t *rec);

  // Check if ReaImGui is available
  bool IsAvailable() const { return m_available; }

  // Show/hide window
  void Show();
  void Hide();
  bool IsVisible() const { return m_visible; }
  void Toggle();

  // Main render loop
  void Render();

  // Get current preferences (for use by other components)
  static StateFilterPreferences GetPreferences();

  // JSFX settings
  static bool GetJSFXIncludeDescription();

private:
  // ReaImGui function pointers
  void *(*m_ImGui_CreateContext)(const char *label, int *config_flagsInOptional);
  bool (*m_ImGui_Begin)(void *ctx, const char *name, bool *p_openInOutOptional,
                        int *flagsInOptional);
  void (*m_ImGui_End)(void *ctx);
  void (*m_ImGui_SetNextWindowSize)(void *ctx, double size_w, double size_h, int *condInOptional);
  void (*m_ImGui_Text)(void *ctx, const char *text);
  void (*m_ImGui_TextColored)(void *ctx, int col_rgba, const char *text);
  bool (*m_ImGui_Button)(void *ctx, const char *label, double *size_wInOptional,
                         double *size_hInOptional);
  void (*m_ImGui_SameLine)(void *ctx, double *offset_from_start_xInOptional,
                           double *spacingInOptional);
  void (*m_ImGui_Separator)(void *ctx);
  void (*m_ImGui_Spacing)(void *ctx);
  bool (*m_ImGui_Checkbox)(void *ctx, const char *label, bool *v);
  bool (*m_ImGui_BeginCombo)(void *ctx, const char *label, const char *preview_value,
                             int *flagsInOptional);
  void (*m_ImGui_EndCombo)(void *ctx);
  bool (*m_ImGui_Selectable)(void *ctx, const char *label, bool *p_selectedInOut,
                             int *flagsInOptional, double *size_wInOptional,
                             double *size_hInOptional);
  bool (*m_ImGui_InputInt)(void *ctx, const char *label, int *v, int *stepInOptional,
                           int *step_fastInOptional, int *flagsInOptional);
  bool (*m_ImGui_InputText)(void *ctx, const char *label, char *buf, int buf_size,
                            int *flagsInOptional, void *callbackInOptional,
                            void *user_dataInOptional);
  bool (*m_ImGui_InputTextWithHint)(void *ctx, const char *label, const char *hint, char *buf,
                                    int buf_size, int *flagsInOptional, void *callbackInOptional,
                                    void *user_dataInOptional);
  void (*m_ImGui_PushItemWidth)(void *ctx, double item_width);
  void (*m_ImGui_PopItemWidth)(void *ctx);
  void (*m_ImGui_GetContentRegionAvail)(void *ctx, double *wOut, double *hOut);
  bool (*m_ImGui_IsWindowAppearing)(void *ctx);
  void (*m_ImGui_SetKeyboardFocusHere)(void *ctx, int *offsetInOptional);
  void (*m_ImGui_PushStyleColor)(void *ctx, int idx, int col_rgba);
  void (*m_ImGui_PopStyleColor)(void *ctx, int *countInOptional);

  // State
  bool m_available = false;
  bool m_visible = false;
  bool m_shouldClose = false; // Deferred close flag
  void *m_ctx = nullptr;

  // Current preferences (editable in UI)
  int m_filterModeIndex = 0;
  bool m_includeEmptyTracks = true;
  int m_maxClipsPerTrack = 0;

  // JSFX settings
  bool m_jsfxIncludeDescription = true;

  // Internal methods
  void LoadSettings();
  void SaveSettings();
  void OnSave();
};

// Global instance
extern MagdaImGuiSettings *g_imguiSettings;
