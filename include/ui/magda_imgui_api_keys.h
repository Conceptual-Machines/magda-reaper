#pragma once

#include "reaper_plugin.h"
#include <string>

// API Key validation status
enum class ApiKeyStatus {
  Unknown,  // Not yet validated
  Checking, // Currently validating
  Valid,    // Key is valid
  Invalid,  // Key is invalid
  Error     // Validation failed (network error, etc.)
};

class MagdaImGuiApiKeys {
public:
  MagdaImGuiApiKeys();
  ~MagdaImGuiApiKeys();

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

  // Static methods to get/set API keys
  static const char *GetOpenAIApiKey();
  static void SetOpenAIApiKey(const char *key);

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
  bool (*m_ImGui_InputText)(void *ctx, const char *label, char *buf, int buf_size,
                            int *flagsInOptional, void *callbackInOptional);
  bool (*m_ImGui_InputTextWithHint)(void *ctx, const char *label, const char *hint, char *buf,
                                    int buf_size, int *flagsInOptional, void *callbackInOptional);
  void (*m_ImGui_PushItemWidth)(void *ctx, double item_width);
  void (*m_ImGui_PopItemWidth)(void *ctx);
  void (*m_ImGui_PushStyleColor)(void *ctx, int idx, int col_rgba);
  void (*m_ImGui_PopStyleColor)(void *ctx, int *countInOptional);

  // ReaImGui constant getter functions
  int (*m_ImGui_InputTextFlags_Password)();

  // State
  bool m_available = false;
  bool m_visible = false;
  void *m_ctx = nullptr;

  // API Key input
  char m_openaiApiKey[256] = {0};
  bool m_showKey = false;
  ApiKeyStatus m_openaiKeyStatus = ApiKeyStatus::Unknown;
  std::string m_statusMessage;

  // Validation
  void ValidateOpenAIKey();
  void OnValidationComplete(bool success, const char *message);

  // Internal methods
  void LoadSettings();
  void SaveSettings();
};

// Global instance
extern MagdaImGuiApiKeys *g_imguiApiKeys;
