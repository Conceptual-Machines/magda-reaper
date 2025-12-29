#include "magda_imgui_settings.h"
#include <cstdlib>
#include <cstring>

// g_rec is needed for REAPER API access
extern reaper_plugin_info_t *g_rec;

// ReaImGui constants
namespace ImGuiCond {
constexpr int FirstUseEver = 1 << 2;
} // namespace ImGuiCond

namespace ImGuiWindowFlags {
constexpr int NoCollapse = 1 << 5;
} // namespace ImGuiWindowFlags

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
} // namespace ImGuiCol

// Theme colors - format is 0xRRGGBBAA (matches chat/login windows)
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
  int accent = THEME_RGBA(0x3D, 0x5A, 0xFE);       // Electric blue
  int accentHover = THEME_RGBA(0x53, 0x6D, 0xFE);  // Lighter on hover
  int accentActive = THEME_RGBA(0x2A, 0x45, 0xD0); // Darker on press
  // Title bar - dark grey to match window
  int titleBg = THEME_RGBA(0x2D, 0x2D, 0x2D);       // Inactive title
  int titleBgActive = THEME_RGBA(0x3C, 0x3C, 0x3C); // Active title
};
static ThemeColors g_theme;

static const int COLOR_DIM = THEME_RGBA(0x80, 0x80, 0x80);

// Filter mode names for combo box
static const char *FILTER_MODE_NAMES[] = {
    "All tracks and clips", "Selected tracks only",
    "Selected tracks + selected clips", "Selected clips only"};
static const int FILTER_MODE_COUNT = 4;

MagdaImGuiSettings::MagdaImGuiSettings() { LoadSettings(); }

MagdaImGuiSettings::~MagdaImGuiSettings() {}

bool MagdaImGuiSettings::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load ReaImGui function pointers
  m_ImGui_CreateContext =
      (void *(*)(const char *, int *))rec->GetFunc("ImGui_CreateContext");
  m_ImGui_Begin = (bool (*)(void *, const char *, bool *, int *))rec->GetFunc(
      "ImGui_Begin");
  m_ImGui_End = (void (*)(void *))rec->GetFunc("ImGui_End");
  m_ImGui_SetNextWindowSize = (void (*)(
      void *, double, double, int *))rec->GetFunc("ImGui_SetNextWindowSize");
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
  m_ImGui_BeginCombo = (bool (*)(void *, const char *, const char *,
                                 int *))rec->GetFunc("ImGui_BeginCombo");
  m_ImGui_EndCombo = (void (*)(void *))rec->GetFunc("ImGui_EndCombo");
  m_ImGui_Selectable = (bool (*)(void *, const char *, bool *, int *, double *,
                                 double *))rec->GetFunc("ImGui_Selectable");
  m_ImGui_InputInt = (bool (*)(void *, const char *, int *, int *, int *,
                               int *))rec->GetFunc("ImGui_InputInt");
  m_ImGui_InputText =
      (bool (*)(void *, const char *, char *, int, int *, void *,
                void *))rec->GetFunc("ImGui_InputText");
  m_ImGui_InputTextWithHint =
      (bool (*)(void *, const char *, const char *, char *, int, int *, void *,
                void *))rec->GetFunc("ImGui_InputTextWithHint");
  m_ImGui_PushItemWidth =
      (void (*)(void *, double))rec->GetFunc("ImGui_PushItemWidth");
  m_ImGui_PopItemWidth = (void (*)(void *))rec->GetFunc("ImGui_PopItemWidth");
  m_ImGui_GetContentRegionAvail = (void (*)(
      void *, double *, double *))rec->GetFunc("ImGui_GetContentRegionAvail");
  m_ImGui_IsWindowAppearing =
      (bool (*)(void *))rec->GetFunc("ImGui_IsWindowAppearing");
  m_ImGui_SetKeyboardFocusHere =
      (void (*)(void *, int *))rec->GetFunc("ImGui_SetKeyboardFocusHere");
  m_ImGui_PushStyleColor =
      (void (*)(void *, int, int))rec->GetFunc("ImGui_PushStyleColor");
  m_ImGui_PopStyleColor =
      (void (*)(void *, int *))rec->GetFunc("ImGui_PopStyleColor");

  // Check if essential functions are available
  m_available = m_ImGui_CreateContext && m_ImGui_Begin && m_ImGui_End &&
                m_ImGui_Text && m_ImGui_Button && m_ImGui_Checkbox &&
                m_ImGui_BeginCombo && m_ImGui_EndCombo && m_ImGui_Selectable;

  return m_available;
}

void MagdaImGuiSettings::LoadSettings() {
  if (!g_rec)
    return;

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState)
    return;

  // Load filter mode
  const char *modeStr = GetExtState("MAGDA", "state_filter_mode");
  if (modeStr) {
    int mode = atoi(modeStr);
    if (mode >= 0 && mode <= 3) {
      m_filterModeIndex = mode;
    }
  }

  // Load include empty tracks
  const char *includeEmptyStr =
      GetExtState("MAGDA", "state_filter_include_empty");
  if (includeEmptyStr) {
    m_includeEmptyTracks = (atoi(includeEmptyStr) != 0);
  }

  // Load max clips per track
  const char *maxClipsStr = GetExtState("MAGDA", "state_filter_max_clips");
  if (maxClipsStr) {
    m_maxClipsPerTrack = atoi(maxClipsStr);
  }

  // Load JSFX include description setting
  const char *jsfxDescStr = GetExtState("MAGDA", "jsfx_include_description");
  if (jsfxDescStr) {
    m_jsfxIncludeDescription = (atoi(jsfxDescStr) != 0);
  }
}

void MagdaImGuiSettings::SaveSettings() {
  if (!g_rec)
    return;

  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState)
    return;

  // Save filter mode
  char modeStr[32];
  snprintf(modeStr, sizeof(modeStr), "%d", m_filterModeIndex);
  SetExtState("MAGDA", "state_filter_mode", modeStr, true);

  // Save include empty tracks
  SetExtState("MAGDA", "state_filter_include_empty",
              m_includeEmptyTracks ? "1" : "0", true);

  // Save max clips per track
  char maxClipsStr[32];
  snprintf(maxClipsStr, sizeof(maxClipsStr), "%d", m_maxClipsPerTrack);
  SetExtState("MAGDA", "state_filter_max_clips", maxClipsStr, true);

  // Save JSFX include description setting
  SetExtState("MAGDA", "jsfx_include_description",
              m_jsfxIncludeDescription ? "1" : "0", true);
}

StateFilterPreferences MagdaImGuiSettings::GetPreferences() {
  StateFilterPreferences prefs;

  if (!g_rec)
    return prefs;

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState)
    return prefs;

  // Load filter mode
  const char *modeStr = GetExtState("MAGDA", "state_filter_mode");
  if (modeStr) {
    int mode = atoi(modeStr);
    if (mode >= 0 && mode <= 3) {
      prefs.mode = static_cast<StateFilterMode>(mode);
    }
  }

  // Load include empty tracks
  const char *includeEmptyStr =
      GetExtState("MAGDA", "state_filter_include_empty");
  if (includeEmptyStr) {
    prefs.includeEmptyTracks = (atoi(includeEmptyStr) != 0);
  }

  // Load max clips per track
  const char *maxClipsStr = GetExtState("MAGDA", "state_filter_max_clips");
  if (maxClipsStr) {
    prefs.maxClipsPerTrack = atoi(maxClipsStr);
  }

  return prefs;
}

bool MagdaImGuiSettings::GetJSFXIncludeDescription() {
  if (!g_rec)
    return true; // Default to including description

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState)
    return true;

  const char *jsfxDescStr = GetExtState("MAGDA", "jsfx_include_description");
  if (jsfxDescStr) {
    return (atoi(jsfxDescStr) != 0);
  }
  return true; // Default to including description
}

void MagdaImGuiSettings::Show() {
  m_visible = true;
  // Recreate context if it was destroyed (e.g., when window was closed)
  if (!m_ctx && m_available) {
    m_ctx = m_ImGui_CreateContext("MAGDA Settings", nullptr);
  }
}

void MagdaImGuiSettings::Hide() {
  m_shouldClose = true;  // Will close on next frame
}

void MagdaImGuiSettings::Toggle() {
  if (m_visible) {
    Hide();
  } else {
    Show();
  }
}

void MagdaImGuiSettings::OnSave() {
  SaveSettings();
  // Don't call Hide() here - let the window close naturally
  // Just set a flag to close on next frame
  m_shouldClose = true;
}

void MagdaImGuiSettings::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    m_ctx = m_ImGui_CreateContext("MAGDA Settings", nullptr);
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
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBgHovered,
                           g_theme.buttonHover);
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
    // Title bar colors
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBg, g_theme.titleBg);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBgActive,
                           g_theme.titleBgActive);
    styleColorCount++;
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBgCollapsed, g_theme.titleBg);
    styleColorCount++;
  }

  // Set window size
  int cond = ImGuiCond::FirstUseEver;
  m_ImGui_SetNextWindowSize(m_ctx, 450, 280, &cond);

  // Begin window
  int flags = ImGuiWindowFlags::NoCollapse;
  bool open = !m_shouldClose;  // If should close, start with open=false
  m_shouldClose = false;  // Reset flag
  if (!m_ImGui_Begin(m_ctx, "MAGDA Settings", &open, &flags)) {
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

  // State filtering section
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, g_theme.headerText, "State Filtering");
  } else {
    m_ImGui_Text(m_ctx, "State Filtering");
  }
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Help text
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, COLOR_DIM,
                        "Control what REAPER state is sent to the API");
  }
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Filter mode combo
  double availW = 0, availH = 0;
  if (m_ImGui_GetContentRegionAvail) {
    m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);
  }

  if (m_ImGui_PushItemWidth) {
    m_ImGui_PushItemWidth(m_ctx, 220); // Smaller fixed width for dropdown
  }

  // Filter mode dropdown using BeginCombo/EndCombo
  const char *currentModeName =
      (m_filterModeIndex >= 0 && m_filterModeIndex < FILTER_MODE_COUNT)
          ? FILTER_MODE_NAMES[m_filterModeIndex]
          : "Unknown";

  // Use ## to hide label (it's explained by the section header)
  if (m_ImGui_BeginCombo(m_ctx, "##filter_mode", currentModeName, nullptr)) {
    for (int i = 0; i < FILTER_MODE_COUNT; i++) {
      bool isSelected = (m_filterModeIndex == i);
      if (m_ImGui_Selectable(m_ctx, FILTER_MODE_NAMES[i], &isSelected, nullptr,
                             nullptr, nullptr)) {
        m_filterModeIndex = i;
      }
    }
    m_ImGui_EndCombo(m_ctx);
  }

  if (m_ImGui_PopItemWidth) {
    m_ImGui_PopItemWidth(m_ctx);
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Include empty tracks checkbox
  if (m_ImGui_Checkbox) {
    m_ImGui_Checkbox(m_ctx, "Include empty tracks", &m_includeEmptyTracks);
  }

  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Max clips per track input
  m_ImGui_Text(m_ctx, "Max clips per track (0 = unlimited):");
  if (m_ImGui_PushItemWidth) {
    m_ImGui_PushItemWidth(m_ctx, 100);
  }
  if (m_ImGui_InputInt) {
    int step = 1;
    int stepFast = 10;
    m_ImGui_InputInt(m_ctx, "##maxclips", &m_maxClipsPerTrack, &step, &stepFast,
                     nullptr);
    if (m_maxClipsPerTrack < 0)
      m_maxClipsPerTrack = 0;
  }
  if (m_ImGui_PopItemWidth) {
    m_ImGui_PopItemWidth(m_ctx);
  }

  if (m_ImGui_Separator)
    m_ImGui_Separator(m_ctx);
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // JSFX Settings section
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, g_theme.headerText, "JSFX Editor");
  } else {
    m_ImGui_Text(m_ctx, "JSFX Editor");
  }
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Include description in AI-generated JSFX
  if (m_ImGui_Checkbox) {
    m_ImGui_Checkbox(m_ctx, "Include description in AI-generated JSFX",
                     &m_jsfxIncludeDescription);
  }
  if (m_ImGui_TextColored) {
    m_ImGui_TextColored(m_ctx, COLOR_DIM,
                        "When enabled, AI explains the generated code");
  }

  if (m_ImGui_Separator)
    m_ImGui_Separator(m_ctx);
  if (m_ImGui_Spacing)
    m_ImGui_Spacing(m_ctx);

  // Save button - accent colored
  if (m_ImGui_PushStyleColor) {
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, g_theme.accent);
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonHovered, g_theme.accentHover);
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonActive, g_theme.accentActive);
  }

  if (m_ImGui_Button(m_ctx, "Save", nullptr, nullptr)) {
    OnSave();
  }

  if (m_ImGui_PopStyleColor) {
    int n = 3;
    m_ImGui_PopStyleColor(m_ctx, &n);
  }

  if (m_ImGui_SameLine)
    m_ImGui_SameLine(m_ctx, nullptr, nullptr);

  if (m_ImGui_Button(m_ctx, "Cancel", nullptr, nullptr)) {
    LoadSettings(); // Revert changes
    Hide();
  }

  m_ImGui_End(m_ctx);

  // Pop window style colors
  if (m_ImGui_PopStyleColor) {
    m_ImGui_PopStyleColor(m_ctx, &styleColorCount);
  }
}
