#include "magda_imgui_login.h"
#include "magda_api_client.h"
#include "magda_auth.h"
#include "magda_env.h"
#include <cstring>

// g_rec is needed for REAPER API access
extern reaper_plugin_info_t *g_rec;

// ReaImGui constants
namespace ImGuiCond {
constexpr int FirstUseEver = 1 << 2;
constexpr int Always = 1 << 3;
} // namespace ImGuiCond

namespace ImGuiWindowFlags {
constexpr int NoCollapse = 1 << 5;
constexpr int NoResize = 1 << 1;
constexpr int AlwaysAutoResize = 1 << 6;
} // namespace ImGuiWindowFlags

namespace ImGuiInputTextFlags {
constexpr int EnterReturnsTrue = 1 << 5;
constexpr int Password = 1 << 15;
} // namespace ImGuiInputTextFlags

namespace ImGuiCol {
constexpr int Text = 0;
constexpr int WindowBg = 2;
constexpr int FrameBg = 7;
constexpr int Button = 21;
constexpr int ButtonHovered = 22;
constexpr int ButtonActive = 23;
} // namespace ImGuiCol

// Theme colors - format is 0xRRGGBBAA
#define THEME_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xFF)

static const int COLOR_SUCCESS = THEME_RGBA(0x88, 0xFF, 0x88);
static const int COLOR_ERROR = THEME_RGBA(0xFF, 0x66, 0x66);
static const int COLOR_WARNING = THEME_RGBA(0xFF, 0xFF, 0x66);
static const int COLOR_INFO = THEME_RGBA(0x52, 0x94, 0xE2);
static const int COLOR_DIM = THEME_RGBA(0x90, 0x90, 0x90);

// Default API URL for local development
static const char *DEFAULT_API_URL = "http://localhost:8080";

MagdaImGuiLogin::MagdaImGuiLogin() {
  // Set default API URL
  strncpy(m_apiUrlBuffer, DEFAULT_API_URL, sizeof(m_apiUrlBuffer) - 1);
}

MagdaImGuiLogin::~MagdaImGuiLogin() {
  // Wait for any pending async operation
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }
}

bool MagdaImGuiLogin::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load ReaImGui function pointers
  m_ImGui_CreateContext = (void *(*)(const char *, int *))rec->GetFunc(
      "ImGui_CreateContext");
  m_ImGui_Begin = (bool (*)(void *, const char *, bool *, int *))rec->GetFunc(
      "ImGui_Begin");
  m_ImGui_End = (void (*)(void *))rec->GetFunc("ImGui_End");
  m_ImGui_SetNextWindowSize =
      (void (*)(void *, double, double, int *))rec->GetFunc(
          "ImGui_SetNextWindowSize");
  m_ImGui_Text = (void (*)(void *, const char *))rec->GetFunc("ImGui_Text");
  m_ImGui_TextColored =
      (void (*)(void *, int, const char *))rec->GetFunc("ImGui_TextColored");
  m_ImGui_InputText =
      (bool (*)(void *, const char *, char *, int, int *, void *))rec->GetFunc(
          "ImGui_InputText");
  m_ImGui_InputTextWithHint =
      (bool (*)(void *, const char *, const char *, char *, int, int *,
                void *))rec->GetFunc("ImGui_InputTextWithHint");
  m_ImGui_Button = (bool (*)(void *, const char *, double *, double *))
                       rec->GetFunc("ImGui_Button");
  m_ImGui_SameLine =
      (void (*)(void *, double *, double *))rec->GetFunc("ImGui_SameLine");
  m_ImGui_Separator = (void (*)(void *))rec->GetFunc("ImGui_Separator");
  m_ImGui_Spacing = (void (*)(void *))rec->GetFunc("ImGui_Spacing");
  m_ImGui_Dummy =
      (void (*)(void *, double, double))rec->GetFunc("ImGui_Dummy");
  m_ImGui_PushStyleColor =
      (void (*)(void *, int, int))rec->GetFunc("ImGui_PushStyleColor");
  m_ImGui_PopStyleColor =
      (void (*)(void *, int *))rec->GetFunc("ImGui_PopStyleColor");
  m_ImGui_PushItemWidth =
      (void (*)(void *, double))rec->GetFunc("ImGui_PushItemWidth");
  m_ImGui_PopItemWidth = (void (*)(void *))rec->GetFunc("ImGui_PopItemWidth");
  m_ImGui_IsWindowAppearing =
      (bool (*)(void *))rec->GetFunc("ImGui_IsWindowAppearing");
  m_ImGui_SetKeyboardFocusHere =
      (void (*)(void *, int *))rec->GetFunc("ImGui_SetKeyboardFocusHere");
  m_ImGui_GetContentRegionAvail =
      (void (*)(void *, double *, double *))rec->GetFunc(
          "ImGui_GetContentRegionAvail");
  m_ImGui_BeginDisabled =
      (bool (*)(void *, bool *))rec->GetFunc("ImGui_BeginDisabled");
  m_ImGui_EndDisabled = (void (*)(void *))rec->GetFunc("ImGui_EndDisabled");

  // Check if essential functions are available
  m_available = m_ImGui_CreateContext && m_ImGui_Begin && m_ImGui_End &&
                m_ImGui_Text && m_ImGui_InputText && m_ImGui_Button;

  if (m_available) {
    // Load saved settings
    LoadSettings();
  }

  return m_available;
}

void MagdaImGuiLogin::LoadSettings() {
  if (!g_rec)
    return;

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState)
    return;

  // Load API URL
  const char *storedUrl = GetExtState("MAGDA", "api_url");
  if (storedUrl && strlen(storedUrl) > 0) {
    strncpy(m_apiUrlBuffer, storedUrl, sizeof(m_apiUrlBuffer) - 1);
  } else {
    // Check .env fallback
    const char *envUrl = MagdaEnv::Get("MAGDA_BACKEND_URL", "");
    if (strlen(envUrl) > 0) {
      strncpy(m_apiUrlBuffer, envUrl, sizeof(m_apiUrlBuffer) - 1);
    }
  }

  // Load email (for convenience, not password)
  const char *storedEmail = GetExtState("MAGDA", "email");
  if (storedEmail && strlen(storedEmail) > 0) {
    strncpy(m_emailBuffer, storedEmail, sizeof(m_emailBuffer) - 1);
  }

  // Check if we have a stored token
  const char *token = GetStoredToken();
  if (token && strlen(token) > 0) {
    m_loggedIn = true;
  }
}

void MagdaImGuiLogin::SaveSettings() {
  if (!g_rec)
    return;

  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState)
    return;

  // Save API URL
  SetExtState("MAGDA", "api_url", m_apiUrlBuffer, true);

  // Save email (for convenience)
  SetExtState("MAGDA", "email", m_emailBuffer, true);
}

const char *MagdaImGuiLogin::GetStoredToken() {
  return MagdaAuth::GetStoredToken();
}

void MagdaImGuiLogin::StoreToken(const char *token) {
  MagdaAuth::StoreToken(token);
}

const char *MagdaImGuiLogin::GetAPIUrl() const {
  if (strlen(m_apiUrlBuffer) > 0) {
    return m_apiUrlBuffer;
  }
  return DEFAULT_API_URL;
}

const char *MagdaImGuiLogin::GetBackendURL() {
  // Use the global instance if available
  extern MagdaImGuiLogin *g_imguiLogin;
  if (g_imguiLogin) {
    return g_imguiLogin->GetAPIUrl();
  }

  // Fall back to environment variable or default
  const char *env_url = MagdaEnv::Get("MAGDA_BACKEND_URL", "");
  if (env_url && strlen(env_url) > 0) {
    return env_url;
  }

  // Fall back to ExtState
  if (g_rec) {
    const char *(*GetExtState)(const char *, const char *) =
        (const char *(*)(const char *, const char *))g_rec->GetFunc(
            "GetExtState");
    if (GetExtState) {
      const char *stored = GetExtState("MAGDA", "api_url");
      if (stored && strlen(stored) > 0) {
        return stored;
      }
    }
  }

  return DEFAULT_API_URL;
}

void MagdaImGuiLogin::SetAPIUrl(const char *url) {
  if (url) {
    strncpy(m_apiUrlBuffer, url, sizeof(m_apiUrlBuffer) - 1);
    SaveSettings();
  }
}

void MagdaImGuiLogin::Show() {
  m_visible = true;

  // Check API health when window opens
  if (m_authMode == AuthMode::Unknown && !m_checkingHealth) {
    StartHealthCheck();
  }
}

void MagdaImGuiLogin::Hide() { m_visible = false; }

void MagdaImGuiLogin::Toggle() {
  if (m_visible) {
    Hide();
  } else {
    Show();
  }
}

void MagdaImGuiLogin::CheckAPIHealth() { StartHealthCheck(); }

void MagdaImGuiLogin::StartHealthCheck() {
  if (m_asyncPending)
    return;

  m_checkingHealth = true;
  m_statusMessage = "Checking API...";
  m_statusIsError = false;

  // Start health check in background thread
  m_asyncPending = true;
  std::string apiUrl = m_apiUrlBuffer;

  m_asyncThread = std::thread([this, apiUrl]() {
    // Create a local HTTP client for this thread
    MagdaHTTPClient httpClient;
    httpClient.SetBackendURL(apiUrl.c_str());

    WDL_FastString errorMsg;
    bool success = httpClient.CheckHealth(errorMsg, 5);

    std::lock_guard<std::mutex> lock(m_asyncMutex);

    if (success) {
      // For now, assume local (localhost) = no auth, remote = auth required
      if (apiUrl.find("localhost") != std::string::npos ||
          apiUrl.find("127.0.0.1") != std::string::npos) {
        m_asyncAuthMode = AuthMode::None;
      } else {
        m_asyncAuthMode = AuthMode::Gateway;
      }
      m_asyncSuccess = true;
    } else {
      m_asyncAuthMode = AuthMode::Error;
      m_asyncErrorMsg = errorMsg.GetLength() > 0 ? errorMsg.Get() : "Failed to connect to API";
      m_asyncSuccess = false;
    }

    m_asyncResultReady = true;
    m_asyncPending = false;
  });

  m_asyncThread.detach();
}

void MagdaImGuiLogin::StartLoginRequest() {
  if (m_asyncPending)
    return;

  if (strlen(m_emailBuffer) == 0 || strlen(m_passwordBuffer) == 0) {
    m_statusMessage = "Please enter email and password";
    m_statusIsError = true;
    return;
  }

  m_statusMessage = "Logging in...";
  m_statusIsError = false;

  // Save email for next time
  SaveSettings();

  // Start login in background thread
  m_asyncPending = true;
  std::string email = m_emailBuffer;
  std::string password = m_passwordBuffer;
  std::string apiUrl = m_apiUrlBuffer;

  m_asyncThread = std::thread([this, email, password, apiUrl]() {
    // Create a local HTTP client for this thread
    MagdaHTTPClient httpClient;
    httpClient.SetBackendURL(apiUrl.c_str());

    WDL_FastString tokenOut, errorMsg;
    bool success = httpClient.SendLoginRequest(email.c_str(), password.c_str(),
                                               tokenOut, errorMsg);

    std::lock_guard<std::mutex> lock(m_asyncMutex);

    if (success && tokenOut.GetLength() > 0) {
      m_asyncToken = tokenOut.Get();
      m_asyncSuccess = true;
    } else {
      m_asyncSuccess = false;
      m_asyncErrorMsg = errorMsg.GetLength() > 0 ? errorMsg.Get() : "Login failed";
    }

    m_asyncResultReady = true;
    m_asyncPending = false;
  });

  m_asyncThread.detach();
}

void MagdaImGuiLogin::ProcessAsyncResult() {
  std::lock_guard<std::mutex> lock(m_asyncMutex);

  if (!m_asyncResultReady)
    return;

  m_asyncResultReady = false;
  m_checkingHealth = false;

  if (m_asyncAuthMode != AuthMode::Unknown) {
    // Health check result
    m_authMode = m_asyncAuthMode;
    if (m_authMode == AuthMode::None) {
      m_statusMessage = "Connected (no auth required)";
      m_statusIsError = false;
      m_loggedIn = true; // Auto-login for local
    } else if (m_authMode == AuthMode::Gateway) {
      if (m_loggedIn) {
        m_statusMessage = "Connected (logged in)";
      } else {
        m_statusMessage = "Connected (login required)";
      }
      m_statusIsError = false;
    } else {
      m_statusMessage = m_asyncErrorMsg;
      m_statusIsError = true;
    }
    m_asyncAuthMode = AuthMode::Unknown;
  } else if (!m_asyncToken.empty()) {
    // Login result
    StoreToken(m_asyncToken.c_str());
    m_loggedIn = true;
    m_statusMessage = "Login successful!";
    m_statusIsError = false;
    m_asyncToken.clear();
    // Clear password from memory
    memset(m_passwordBuffer, 0, sizeof(m_passwordBuffer));
  } else if (!m_asyncSuccess) {
    // Error
    m_statusMessage = m_asyncErrorMsg;
    m_statusIsError = true;
  }
}

void MagdaImGuiLogin::OnLogin() { StartLoginRequest(); }

void MagdaImGuiLogin::OnLogout() {
  StoreToken(nullptr);
  m_loggedIn = false;
  m_statusMessage = "Logged out";
  m_statusIsError = false;
  memset(m_passwordBuffer, 0, sizeof(m_passwordBuffer));
}

void MagdaImGuiLogin::Render() {
  if (!m_available || !m_visible)
    return;

  // Process any pending async results
  ProcessAsyncResult();

  // Create context if needed
  if (!m_ctx) {
    m_ctx = m_ImGui_CreateContext("MAGDA Login", nullptr);
    if (!m_ctx)
      return;
  }

  // Set window size
  int cond = ImGuiCond::FirstUseEver;
  m_ImGui_SetNextWindowSize(m_ctx, 400, 300, &cond);

  // Begin window
  int flags = ImGuiWindowFlags::NoCollapse;
  bool open = true;
  if (!m_ImGui_Begin(m_ctx, "MAGDA Login", &open, &flags)) {
    m_ImGui_End(m_ctx);
    if (!open)
      m_visible = false;
    return;
  }

  if (!open) {
    m_visible = false;
    m_ImGui_End(m_ctx);
    return;
  }

  // Focus on first input when window appears
  if (m_ImGui_IsWindowAppearing && m_ImGui_IsWindowAppearing(m_ctx)) {
    if (m_ImGui_SetKeyboardFocusHere) {
      int offset = 0;
      m_ImGui_SetKeyboardFocusHere(m_ctx, &offset);
    }
  }

  // Render sections
  RenderAPISection();

  if (m_ImGui_Separator)
    m_ImGui_Separator(m_ctx);
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  RenderAuthSection();

  if (m_ImGui_Separator)
    m_ImGui_Separator(m_ctx);
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  RenderStatusSection();

  m_ImGui_End(m_ctx);
}

void MagdaImGuiLogin::RenderAPISection() {
  m_ImGui_Text(m_ctx, "API Server");
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // API URL input
  double availW = 0, availH = 0;
  if (m_ImGui_GetContentRegionAvail) {
    m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);
  }

  if (m_ImGui_PushItemWidth && availW > 100) {
    m_ImGui_PushItemWidth(m_ctx, availW - 80);
  }

  int flags = 0;
  if (m_ImGui_InputTextWithHint) {
    m_ImGui_InputTextWithHint(m_ctx, "##apiurl", "http://localhost:8080",
                              m_apiUrlBuffer, sizeof(m_apiUrlBuffer), &flags,
                              nullptr);
  } else {
    m_ImGui_InputText(m_ctx, "##apiurl", m_apiUrlBuffer, sizeof(m_apiUrlBuffer),
                      &flags, nullptr);
  }

  if (m_ImGui_PopItemWidth) {
    m_ImGui_PopItemWidth(m_ctx);
  }

  if (m_ImGui_SameLine)
    m_ImGui_SameLine(m_ctx, nullptr, nullptr);

  // Connect button
  bool checkDisabled = m_asyncPending;
  if (checkDisabled && m_ImGui_BeginDisabled) {
    bool disabled = true;
    m_ImGui_BeginDisabled(m_ctx, &disabled);
  }

  if (m_ImGui_Button(m_ctx, m_checkingHealth ? "..." : "Check", nullptr,
                     nullptr)) {
    SaveSettings();
    StartHealthCheck();
  }

  if (checkDisabled && m_ImGui_EndDisabled) {
    m_ImGui_EndDisabled(m_ctx);
  }

  // Show auth mode info
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  if (m_authMode == AuthMode::None) {
    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, COLOR_SUCCESS, "✓ Local mode (no auth)");
    }
  } else if (m_authMode == AuthMode::Gateway) {
    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, COLOR_INFO, "🔒 Hosted mode (auth required)");
    }
  } else if (m_authMode == AuthMode::Error) {
    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, COLOR_ERROR, "✗ Connection failed");
    }
  } else {
    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, COLOR_DIM, "Click 'Check' to test connection");
    }
  }
}

void MagdaImGuiLogin::RenderAuthSection() {
  // Only show auth fields if in gateway mode
  if (m_authMode != AuthMode::Gateway) {
    if (m_authMode == AuthMode::None) {
      if (m_ImGui_TextColored) {
        m_ImGui_TextColored(m_ctx, COLOR_DIM,
                            "Authentication not required for local API");
      }
    }
    return;
  }

  m_ImGui_Text(m_ctx, "Authentication");
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  if (m_loggedIn) {
    // Show logged in state
    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, COLOR_SUCCESS, "✓ Logged in");
    }
    if (m_ImGui_Spacing)
      m_ImGui_Spacing(m_ctx);

    if (strlen(m_emailBuffer) > 0) {
      char emailInfo[300];
      snprintf(emailInfo, sizeof(emailInfo), "Email: %s", m_emailBuffer);
      if (m_ImGui_TextColored) {
        m_ImGui_TextColored(m_ctx, COLOR_DIM, emailInfo);
      }
    }

    if (m_ImGui_Spacing)
      m_ImGui_Spacing(m_ctx);

    // Logout button
    if (m_ImGui_Button(m_ctx, "Logout", nullptr, nullptr)) {
      OnLogout();
    }
  } else {
    // Show login form
    double availW = 0, availH = 0;
    if (m_ImGui_GetContentRegionAvail) {
      m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);
    }

    if (m_ImGui_PushItemWidth && availW > 50) {
      m_ImGui_PushItemWidth(m_ctx, availW);
    }

    // Email input
    int flags = 0;
    if (m_ImGui_InputTextWithHint) {
      m_ImGui_InputTextWithHint(m_ctx, "##email", "Email", m_emailBuffer,
                                sizeof(m_emailBuffer), &flags, nullptr);
    } else {
      m_ImGui_Text(m_ctx, "Email:");
      m_ImGui_InputText(m_ctx, "##email", m_emailBuffer, sizeof(m_emailBuffer),
                        &flags, nullptr);
    }

    // Password input
    flags = ImGuiInputTextFlags::Password;
    if (m_ImGui_InputTextWithHint) {
      bool enterPressed = m_ImGui_InputTextWithHint(
          m_ctx, "##password", "Password", m_passwordBuffer,
          sizeof(m_passwordBuffer), &flags, nullptr);
      if (enterPressed) {
        OnLogin();
      }
    } else {
      m_ImGui_Text(m_ctx, "Password:");
      m_ImGui_InputText(m_ctx, "##password", m_passwordBuffer,
                        sizeof(m_passwordBuffer), &flags, nullptr);
    }

    if (m_ImGui_PopItemWidth) {
      m_ImGui_PopItemWidth(m_ctx);
    }

    if (m_ImGui_Spacing)
      m_ImGui_Spacing(m_ctx);

    // Login button
    bool loginDisabled = m_asyncPending;
    if (loginDisabled && m_ImGui_BeginDisabled) {
      bool disabled = true;
      m_ImGui_BeginDisabled(m_ctx, &disabled);
    }

    if (m_ImGui_Button(m_ctx, m_asyncPending ? "Logging in..." : "Login",
                       nullptr, nullptr)) {
      OnLogin();
    }

    if (loginDisabled && m_ImGui_EndDisabled) {
      m_ImGui_EndDisabled(m_ctx);
    }
  }
}

void MagdaImGuiLogin::RenderStatusSection() {
  if (m_statusMessage.empty())
    return;

  int color = m_statusIsError ? COLOR_ERROR : COLOR_SUCCESS;
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, color, m_statusMessage.c_str());
  } else {
    m_ImGui_Text(m_ctx, m_statusMessage.c_str());
  }
}
