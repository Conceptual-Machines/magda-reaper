#pragma once

#include "reaper_plugin.h"
#include <functional>
#include <mutex>
#include <string>
#include <thread>

// Auth mode detected from API
enum class AuthMode {
  Unknown,  // Not yet checked
  None,     // Local/self-hosted - no auth required
  Gateway,  // Hosted - auth required
  Error     // Failed to connect
};

class MagdaImGuiLogin {
public:
  MagdaImGuiLogin();
  ~MagdaImGuiLogin();

  // Initialize ReaImGui function pointers
  // Returns false if ReaImGui is not available
  bool Initialize(reaper_plugin_info_t *rec);

  // Check if ReaImGui is available
  bool IsAvailable() const { return m_available; }

  // Show/hide window
  void Show();
  void Hide();
  bool IsVisible() const { return m_visible; }
  void Toggle();

  // Main render loop - call from timer/defer
  void Render();

  // Get current auth state
  bool IsLoggedIn() const { return m_loggedIn; }
  AuthMode GetAuthMode() const { return m_authMode; }

  // Get/set API URL
  const char *GetAPIUrl() const;
  void SetAPIUrl(const char *url);

  // Get stored JWT token (if logged in)
  static const char *GetStoredToken();
  static void StoreToken(const char *token);

  // Check API health and detect auth mode
  void CheckAPIHealth();

private:
  // ReaImGui function pointers (same pattern as MagdaImGuiChat)
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
  bool (*m_ImGui_InputTextWithHint)(void *ctx, const char *label,
                                    const char *hint, char *buf, int buf_sz,
                                    int *flagsInOptional,
                                    void *callbackInOptional);
  bool (*m_ImGui_Button)(void *ctx, const char *label, double *size_wInOptional,
                         double *size_hInOptional);
  void (*m_ImGui_SameLine)(void *ctx, double *offset_from_start_xInOptional,
                           double *spacingInOptional);
  void (*m_ImGui_Separator)(void *ctx);
  void (*m_ImGui_Spacing)(void *ctx);
  void (*m_ImGui_Dummy)(void *ctx, double size_w, double size_h);
  void (*m_ImGui_PushStyleColor)(void *ctx, int idx, int col_rgba);
  void (*m_ImGui_PopStyleColor)(void *ctx, int *countInOptional);
  void (*m_ImGui_PushItemWidth)(void *ctx, double item_width);
  void (*m_ImGui_PopItemWidth)(void *ctx);
  bool (*m_ImGui_IsWindowAppearing)(void *ctx);
  void (*m_ImGui_SetKeyboardFocusHere)(void *ctx, int *offsetInOptional);
  void (*m_ImGui_GetContentRegionAvail)(void *ctx, double *wOut, double *hOut);
  bool (*m_ImGui_BeginDisabled)(void *ctx, bool *disabledInOptional);
  void (*m_ImGui_EndDisabled)(void *ctx);

  // State
  bool m_available = false;
  bool m_visible = false;
  bool m_loggedIn = false;
  void *m_ctx = nullptr;

  // Auth state
  AuthMode m_authMode = AuthMode::Unknown;
  bool m_checkingHealth = false;

  // Input buffers
  char m_apiUrlBuffer[512] = {0};
  char m_emailBuffer[256] = {0};
  char m_passwordBuffer[256] = {0};

  // Status
  std::string m_statusMessage;
  bool m_statusIsError = false;

  // Async request state
  std::mutex m_asyncMutex;
  std::thread m_asyncThread;
  bool m_asyncPending = false;
  bool m_asyncResultReady = false;
  bool m_asyncSuccess = false;
  std::string m_asyncErrorMsg;
  std::string m_asyncToken;
  AuthMode m_asyncAuthMode = AuthMode::Unknown;

  // Internal methods
  void LoadSettings();
  void SaveSettings();
  void OnLogin();
  void OnLogout();
  void ProcessAsyncResult();
  void StartHealthCheck();
  void StartLoginRequest();

  void RenderAPISection();
  void RenderAuthSection();
  void RenderStatusSection();
};

// Global instance
extern MagdaImGuiLogin *g_imguiLogin;
