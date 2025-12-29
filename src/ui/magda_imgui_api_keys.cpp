#include "magda_imgui_api_keys.h"
#include "magda_openai.h"
#include <cstdlib>
#include <cstring>
#include <thread>

// g_rec is needed for REAPER API access
extern reaper_plugin_info_t *g_rec;

// Global instance is defined in main.cpp

// ReaImGui constants
namespace ImGuiCond {
constexpr int FirstUseEver = 1 << 2;
}

namespace ImGuiWindowFlags {
constexpr int NoCollapse = 1 << 5;
}

// Password flag will be loaded from ReaImGui function

namespace ImGuiCol {
constexpr int Text = 0;
constexpr int WindowBg = 2;
constexpr int ChildBg = 3;
constexpr int Border = 5;
constexpr int FrameBg = 7;
constexpr int FrameBgHovered = 8;
constexpr int FrameBgActive = 9;
constexpr int TitleBg = 10;
constexpr int TitleBgActive = 11;
constexpr int TitleBgCollapsed = 12;
constexpr int Button = 21;
constexpr int ButtonHovered = 22;
constexpr int ButtonActive = 23;
}

// Theme colors
#define THEME_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xFF)

struct ThemeColors {
  int windowBg = THEME_RGBA(0x3C, 0x3C, 0x3C);
  int childBg = THEME_RGBA(0x2D, 0x2D, 0x2D);
  int inputBg = THEME_RGBA(0x1E, 0x1E, 0x1E);
  int headerText = THEME_RGBA(0xF0, 0xF0, 0xF0);
  int normalText = THEME_RGBA(0xD0, 0xD0, 0xD0);
  int dimText = THEME_RGBA(0x80, 0x80, 0x80);
  int buttonBg = THEME_RGBA(0x48, 0x48, 0x48);
  int buttonHover = THEME_RGBA(0x58, 0x58, 0x58);
  int buttonActive = THEME_RGBA(0x38, 0x38, 0x38);
  int border = THEME_RGBA(0x50, 0x50, 0x50);
  int accent = THEME_RGBA(0x3D, 0x5A, 0xFE);
  int accentHover = THEME_RGBA(0x53, 0x6D, 0xFE);
  int accentActive = THEME_RGBA(0x2A, 0x45, 0xD0);
  int success = THEME_RGBA(0x4C, 0xAF, 0x50);
  int error = THEME_RGBA(0xF4, 0x43, 0x36);
  int warning = THEME_RGBA(0xFF, 0x98, 0x00);
  int titleBg = THEME_RGBA(0x2D, 0x2D, 0x2D);
  int titleBgActive = THEME_RGBA(0x3C, 0x3C, 0x3C);
};
static ThemeColors g_theme;

MagdaImGuiApiKeys::MagdaImGuiApiKeys() {
  LoadSettings();
}

MagdaImGuiApiKeys::~MagdaImGuiApiKeys() {}

bool MagdaImGuiApiKeys::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load ReaImGui function pointers
  m_ImGui_CreateContext =
      (void *(*)(const char *, int *))rec->GetFunc("ImGui_CreateContext");
  m_ImGui_Begin = (bool (*)(void *, const char *, bool *, int *))rec->GetFunc(
      "ImGui_Begin");
  m_ImGui_End = (void (*)(void *))rec->GetFunc("ImGui_End");
  m_ImGui_SetNextWindowSize = (void (*)(void *, double, double,
                                        int *))rec->GetFunc("ImGui_SetNextWindowSize");
  m_ImGui_Text = (void (*)(void *, const char *))rec->GetFunc("ImGui_Text");
  m_ImGui_TextColored =
      (void (*)(void *, int, const char *))rec->GetFunc("ImGui_TextColored");
  m_ImGui_Button = (bool (*)(void *, const char *, double *,
                             double *))rec->GetFunc("ImGui_Button");
  m_ImGui_SameLine =
      (void (*)(void *, double *, double *))rec->GetFunc("ImGui_SameLine");
  m_ImGui_Separator = (void (*)(void *))rec->GetFunc("ImGui_Separator");
  m_ImGui_Spacing = (void (*)(void *))rec->GetFunc("ImGui_Spacing");
  m_ImGui_Checkbox =
      (bool (*)(void *, const char *, bool *))rec->GetFunc("ImGui_Checkbox");
  m_ImGui_InputText =
      (bool (*)(void *, const char *, char *, int, int *,
                void *))rec->GetFunc("ImGui_InputText");
  m_ImGui_InputTextWithHint =
      (bool (*)(void *, const char *, const char *, char *, int, int *,
                void *))rec->GetFunc("ImGui_InputTextWithHint");
  m_ImGui_PushItemWidth =
      (void (*)(void *, double))rec->GetFunc("ImGui_PushItemWidth");
  m_ImGui_PopItemWidth = (void (*)(void *))rec->GetFunc("ImGui_PopItemWidth");
  m_ImGui_PushStyleColor =
      (void (*)(void *, int, int))rec->GetFunc("ImGui_PushStyleColor");
  m_ImGui_PopStyleColor =
      (void (*)(void *, int *))rec->GetFunc("ImGui_PopStyleColor");

  // Load constant getter functions
  m_ImGui_InputTextFlags_Password =
      (int (*)())rec->GetFunc("ImGui_InputTextFlags_Password");

  m_available = m_ImGui_CreateContext && m_ImGui_Begin && m_ImGui_End &&
                m_ImGui_Text && m_ImGui_Button && m_ImGui_InputText &&
                m_ImGui_InputTextFlags_Password;

  return m_available;
}

void MagdaImGuiApiKeys::LoadSettings() {
  if (!g_rec)
    return;

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc("GetExtState");
  if (!GetExtState)
    return;

  // Load OpenAI API key
  const char *openaiKey = GetExtState("MAGDA", "openai_api_key");
  if (openaiKey && openaiKey[0]) {
    strncpy(m_openaiApiKey, openaiKey, sizeof(m_openaiApiKey) - 1);
    m_openaiApiKey[sizeof(m_openaiApiKey) - 1] = '\0';

    // If we have a saved key, mark it as valid (was validated before saving)
    m_openaiKeyStatus = ApiKeyStatus::Valid;
    m_statusMessage = "Key loaded from settings";

    // Initialize OpenAI client with saved key
    MagdaOpenAI* openai = GetMagdaOpenAI();
    if (openai) {
      openai->SetAPIKey(m_openaiApiKey);
    }
  }
}

void MagdaImGuiApiKeys::SaveSettings() {
  if (!g_rec)
    return;

  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState)
    return;

  // Only save if key is valid
  if (m_openaiKeyStatus == ApiKeyStatus::Valid) {
    SetExtState("MAGDA", "openai_api_key", m_openaiApiKey, true);

    // Update OpenAI client
    MagdaOpenAI* openai = GetMagdaOpenAI();
    if (openai) {
      openai->SetAPIKey(m_openaiApiKey);
    }
  }
}

const char* MagdaImGuiApiKeys::GetOpenAIApiKey() {
  if (!g_rec)
    return "";

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc("GetExtState");
  if (!GetExtState)
    return "";

  const char *key = GetExtState("MAGDA", "openai_api_key");
  return key ? key : "";
}

void MagdaImGuiApiKeys::SetOpenAIApiKey(const char* key) {
  if (!g_rec)
    return;

  void (*SetExtState)(const char *section, const char *key_name, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState)
    return;

  SetExtState("MAGDA", "openai_api_key", key ? key : "", true);
}

void MagdaImGuiApiKeys::Show() {
  m_visible = true;
  // Recreate context if it was destroyed (e.g., when window was closed)
  if (!m_ctx && m_available) {
    m_ctx = m_ImGui_CreateContext("MAGDA API Keys", nullptr);
  }
  // Reload settings when showing
  LoadSettings();
}

void MagdaImGuiApiKeys::Hide() { m_visible = false; }

void MagdaImGuiApiKeys::Toggle() {
  if (m_visible) {
    Hide();
  } else {
    Show();
  }
}

void MagdaImGuiApiKeys::ValidateOpenAIKey() {
  if (m_openaiKeyStatus == ApiKeyStatus::Checking) {
    return; // Already checking
  }

  if (strlen(m_openaiApiKey) == 0) {
    m_openaiKeyStatus = ApiKeyStatus::Invalid;
    m_statusMessage = "API key cannot be empty";
    return;
  }

  // Basic format check
  if (strncmp(m_openaiApiKey, "sk-", 3) != 0) {
    m_openaiKeyStatus = ApiKeyStatus::Invalid;
    m_statusMessage = "Invalid format (should start with 'sk-')";
    return;
  }

  m_openaiKeyStatus = ApiKeyStatus::Checking;
  m_statusMessage = "Validating...";

  // Create a copy of the key for the thread
  char keyCopy[256];
  strncpy(keyCopy, m_openaiApiKey, sizeof(keyCopy) - 1);
  keyCopy[sizeof(keyCopy) - 1] = '\0';

  // Validate in background thread
  std::thread([this, keyCopy]() {
    MagdaOpenAI* openai = GetMagdaOpenAI();
    if (!openai) {
      OnValidationComplete(false, "OpenAI client not available");
      return;
    }

    // Set the key for validation
    openai->SetAPIKey(keyCopy);

    // Validate by calling GET /v1/models
    WDL_FastString error;
    bool success = openai->ValidateAPIKey(error);

    if (success) {
      OnValidationComplete(true, "API key is valid!");
    } else {
      OnValidationComplete(false, error.GetLength() > 0 ? error.Get() : "Invalid API key");
    }
  }).detach();
}

void MagdaImGuiApiKeys::OnValidationComplete(bool success, const char* message) {
  m_openaiKeyStatus = success ? ApiKeyStatus::Valid : ApiKeyStatus::Invalid;
  m_statusMessage = message ? message : "";

  if (success) {
    SaveSettings();
  }
}

void MagdaImGuiApiKeys::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    m_ctx = m_ImGui_CreateContext("MAGDA API Keys", nullptr);
    if (!m_ctx)
      return;
  }

  // Apply theme colors
  int styleColorCount = 0;
  if (m_ImGui_PushStyleColor) {
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::WindowBg, g_theme.windowBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, g_theme.childBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Text, g_theme.normalText);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBg, g_theme.inputBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBgHovered, g_theme.buttonHover);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBgActive, g_theme.buttonBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, g_theme.buttonBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonHovered, g_theme.buttonHover);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonActive, g_theme.buttonActive);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Border, g_theme.border);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBg, g_theme.titleBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBgActive, g_theme.titleBgActive);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBgCollapsed, g_theme.titleBg);
    styleColorCount++;
  }

  // Set window size
  int cond = ImGuiCond::FirstUseEver;
  m_ImGui_SetNextWindowSize(m_ctx, 450, 200, &cond);

  // Begin window
  int flags = ImGuiWindowFlags::NoCollapse;
  bool open = true;
  if (!m_ImGui_Begin(m_ctx, "MAGDA API Keys", &open, &flags)) {
    m_ImGui_End(m_ctx);
    if (m_ImGui_PopStyleColor) {
      m_ImGui_PopStyleColor(m_ctx, &styleColorCount);
    }
    if (!open) {
      m_visible = false;
      m_ctx = nullptr;
    }
    return;
  }

  if (!open) {
    m_visible = false;
    m_ImGui_End(m_ctx);
    if (m_ImGui_PopStyleColor) {
      m_ImGui_PopStyleColor(m_ctx, &styleColorCount);
    }
    m_ctx = nullptr;
    return;
  }

  // OpenAI API Key section
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, g_theme.headerText, "OpenAI API Key");
  } else {
    m_ImGui_Text(m_ctx, "OpenAI API Key");
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Help text
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, g_theme.dimText,
                        "Required for direct OpenAI mode (no server needed)");
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // API Key input
  if (m_ImGui_PushItemWidth) {
    m_ImGui_PushItemWidth(m_ctx, 320);
  }

  // Use InputText with Password flag from ReaImGui
  m_ImGui_Text(m_ctx, "API Key:");
  int inputFlags = m_ImGui_InputTextFlags_Password();
  if (m_ImGui_InputText) {
    m_ImGui_InputText(m_ctx, "##apikey", m_openaiApiKey,
                      sizeof(m_openaiApiKey), &inputFlags, nullptr);
  }

  if (m_ImGui_PopItemWidth) {
    m_ImGui_PopItemWidth(m_ctx);
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Status display
  if (!m_statusMessage.empty()) {
    int statusColor = g_theme.dimText;
    switch (m_openaiKeyStatus) {
      case ApiKeyStatus::Valid:
        statusColor = g_theme.success;
        break;
      case ApiKeyStatus::Invalid:
      case ApiKeyStatus::Error:
        statusColor = g_theme.error;
        break;
      case ApiKeyStatus::Checking:
        statusColor = g_theme.warning;
        break;
      default:
        break;
    }

    if (m_ImGui_TextColored) {
      m_ImGui_TextColored(m_ctx, statusColor, m_statusMessage.c_str());
    } else {
      m_ImGui_Text(m_ctx, m_statusMessage.c_str());
    }
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);
  if (m_ImGui_Separator)
    m_ImGui_Separator(m_ctx);
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Validate button - accent colored
  bool isChecking = (m_openaiKeyStatus == ApiKeyStatus::Checking);

  if (m_ImGui_PushStyleColor && !isChecking) {
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, g_theme.accent);
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonHovered, g_theme.accentHover);
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonActive, g_theme.accentActive);
  }

  const char* validateLabel = isChecking ? "Validating..." : "Validate & Save";
  if (m_ImGui_Button(m_ctx, validateLabel, nullptr, nullptr) && !isChecking) {
    ValidateOpenAIKey();
  }

  if (m_ImGui_PopStyleColor && !isChecking) {
    int n = 3;
    m_ImGui_PopStyleColor(m_ctx, &n);
  }

  m_ImGui_End(m_ctx);

  // Pop window style colors
  if (m_ImGui_PopStyleColor) {
    m_ImGui_PopStyleColor(m_ctx, &styleColorCount);
  }
}
