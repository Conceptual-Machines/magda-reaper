#pragma once

#include "magda_param_mapping.h"
#include "reaper_plugin.h"
#include <string>
#include <vector>

// ImGui-based parameter mapping editor window
class MagdaParamMappingWindow {
public:
  MagdaParamMappingWindow();
  ~MagdaParamMappingWindow();

  // Initialize ReaImGui function pointers
  bool Initialize(reaper_plugin_info_t *rec);

  // Check availability
  bool IsAvailable() const { return m_available; }

  // Show window for a specific plugin
  void Show(const std::string &plugin_key, const std::string &plugin_name);
  void Hide();
  bool IsVisible() const { return m_visible; }

  // Main render loop
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
  void (*m_ImGui_TextColored)(void *ctx, int col_rgba, const char *text);
  bool (*m_ImGui_InputText)(void *ctx, const char *label, char *buf, int buf_sz,
                            int *flagsInOptional, void *callbackInOptional);
  bool (*m_ImGui_Button)(void *ctx, const char *label, double *size_wInOptional,
                         double *size_hInOptional);
  void (*m_ImGui_SameLine)(void *ctx, double *offset_from_start_xInOptional,
                           double *spacingInOptional);
  void (*m_ImGui_Separator)(void *ctx);
  bool (*m_ImGui_BeginChild)(void *ctx, const char *str_id,
                             double *size_wInOptional, double *size_hInOptional,
                             int *child_flagsInOptional,
                             int *window_flagsInOptional);
  void (*m_ImGui_EndChild)(void *ctx);
  void (*m_ImGui_PushStyleColor)(void *ctx, int idx, int col_rgba);
  void (*m_ImGui_PopStyleColor)(void *ctx, int *countInOptional);
  bool (*m_ImGui_BeginTable)(void *ctx, const char *str_id, int column,
                             int *flagsInOptional,
                             double *outer_size_wInOptional,
                             double *outer_size_hInOptional,
                             double *inner_widthInOptional);
  void (*m_ImGui_EndTable)(void *ctx);
  void (*m_ImGui_TableNextRow)(void *ctx, int *row_flagsInOptional,
                               double *min_row_heightInOptional);
  bool (*m_ImGui_TableNextColumn)(void *ctx);
  void (*m_ImGui_TableSetupColumn)(void *ctx, const char *label,
                                   int *flagsInOptional,
                                   double *init_width_or_weightInOptional,
                                   int *user_idInOptional);
  void (*m_ImGui_TableHeadersRow)(void *ctx);

  // State
  bool m_available = false;
  bool m_visible = false;
  void *m_ctx = nullptr;

  // Current plugin being edited
  std::string m_pluginKey;
  std::string m_pluginName;

  // Plugin parameters (from Reaper)
  struct PluginParam {
    int index;
    std::string name;
    std::string currentAlias; // User-assigned alias
  };
  std::vector<PluginParam> m_params;

  // Search/filter
  char m_searchBuffer[256] = {0};

  // Has changes flag
  bool m_hasChanges = false;

  // Internal methods
  void LoadPluginParams();
  void LoadExistingAliases();
  void SaveMapping();
};

// Global instance
extern MagdaParamMappingWindow *g_paramMappingWindow;
