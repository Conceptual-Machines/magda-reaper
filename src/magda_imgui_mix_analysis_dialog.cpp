#include "magda_imgui_mix_analysis_dialog.h"
#include "reaper_plugin.h"
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// Track type options
const char *MagdaImGuiMixAnalysisDialog::s_trackTypes[] = {
    "drums", "bass",  "guitar",   "synth",      "strings", "vocals",
    "piano", "brass", "woodwind", "percussion", "other"};

const int MagdaImGuiMixAnalysisDialog::s_trackTypeCount =
    sizeof(s_trackTypes) / sizeof(s_trackTypes[0]);

#define LOAD_IMGUI_FUNC(name, type)                                            \
  m_##name = (type)g_rec->GetFunc(#name);                                      \
  if (!m_##name) {                                                             \
    return false;                                                              \
  }

MagdaImGuiMixAnalysisDialog::MagdaImGuiMixAnalysisDialog()
    : m_ImGui_CreateContext(nullptr), m_ImGui_Begin(nullptr),
      m_ImGui_End(nullptr), m_ImGui_SetNextWindowSize(nullptr),
      m_ImGui_Text(nullptr), m_ImGui_Combo(nullptr), m_ImGui_InputText(nullptr),
      m_ImGui_Button(nullptr), m_ImGui_SameLine(nullptr),
      m_ImGui_Separator(nullptr) {}

MagdaImGuiMixAnalysisDialog::~MagdaImGuiMixAnalysisDialog() { m_ctx = nullptr; }

bool MagdaImGuiMixAnalysisDialog::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load required ImGui functions
  LOAD_IMGUI_FUNC(ImGui_CreateContext, void *(*)(const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_Begin, bool (*)(void *, const char *, bool *, int *));
  LOAD_IMGUI_FUNC(ImGui_End, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetNextWindowSize,
                  void (*)(void *, double, double, int *));
  LOAD_IMGUI_FUNC(ImGui_Text, void (*)(void *, const char *));
  LOAD_IMGUI_FUNC(ImGui_Combo, bool (*)(void *, const char *, int *,
                                        const char *const *, int));
  LOAD_IMGUI_FUNC(ImGui_InputText,
                  bool (*)(void *, const char *, char *, int, int *, void *));
  LOAD_IMGUI_FUNC(ImGui_Button,
                  bool (*)(void *, const char *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_SameLine, void (*)(void *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_Separator, void (*)(void *));

  m_available = true;
  return true;
}

void MagdaImGuiMixAnalysisDialog::Show() {
  if (!m_available) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg(
          "MAGDA: Mix analysis dialog not available (ReaImGui required)\n");
    }
    return;
  }

  m_visible = true;
  m_completed = false;
  m_dialogResult.cancelled = true; // Default to cancelled

  // Reset state
  m_selectedTrackType = 0;
  m_userQueryBuffer[0] = '\0';

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Showing mix analysis dialog\n");
  }
}

void MagdaImGuiMixAnalysisDialog::Reset() {
  m_completed = false;
  m_visible = false;
  m_dialogResult.cancelled = true;
  m_dialogResult.trackType.Set("");
  m_dialogResult.userQuery.Set("");
  m_selectedTrackType = 0;
  m_userQueryBuffer[0] = '\0';
}

void MagdaImGuiMixAnalysisDialog::Render() {
  if (!m_available || !m_visible) {
    return;
  }

  // Validate function pointers
  if (!m_ImGui_CreateContext || !m_ImGui_Begin || !m_ImGui_End ||
      !m_ImGui_SetNextWindowSize) {
    return;
  }

  // Create context on first use
  if (!m_ctx) {
    int configFlags = 0;
    m_ctx = m_ImGui_CreateContext("MAGDA_MixAnalysis", &configFlags);
    if (!m_ctx) {
      return;
    }
  }

  // Set window size (position will be handled by ImGui automatically)
  double windowWidth = 450.0;
  double windowHeight = 200.0;
  int cond = 1 << 2; // ImGuiCond_FirstUseEver
  m_ImGui_SetNextWindowSize(m_ctx, windowWidth, windowHeight, &cond);

  // Window flags: modal-like behavior
  int flags = 1 << 5; // ImGuiWindowFlags_NoCollapse
  bool open = true;
  bool visible = m_ImGui_Begin(m_ctx, "Mix Analysis", &open, &flags);

  if (!visible) {
    m_ImGui_End(m_ctx);
    return;
  }

  // Track type combo
  m_ImGui_Text(m_ctx, "Track Type:");
  m_ImGui_SameLine(m_ctx, nullptr, nullptr);
  m_ImGui_Combo(m_ctx, "##tracktype", &m_selectedTrackType, s_trackTypes,
                s_trackTypeCount);

  m_ImGui_Separator(m_ctx);

  // User query input
  m_ImGui_Text(m_ctx, "Query (optional):");
  int inputFlags = 0;
  m_ImGui_InputText(m_ctx, "##query", m_userQueryBuffer,
                    sizeof(m_userQueryBuffer), &inputFlags, nullptr);

  m_ImGui_Separator(m_ctx);

  // Buttons
  double btnWidth = 80.0;
  double btnHeight = 30.0;

  // Analyze button
  if (m_ImGui_Button(m_ctx, "Analyze", &btnWidth, &btnHeight)) {
    // User clicked OK
    m_dialogResult.cancelled = false;
    if (m_selectedTrackType >= 0 && m_selectedTrackType < s_trackTypeCount) {
      m_dialogResult.trackType.Set(s_trackTypes[m_selectedTrackType]);
    } else {
      m_dialogResult.trackType.Set("other");
    }
    m_dialogResult.userQuery.Set(m_userQueryBuffer);
    m_completed = true;
    m_visible = false;
    open = false;
  }

  m_ImGui_SameLine(m_ctx, nullptr, nullptr);

  // Cancel button
  if (m_ImGui_Button(m_ctx, "Cancel", &btnWidth, &btnHeight)) {
    // User clicked Cancel
    m_dialogResult.cancelled = true;
    m_completed = true;
    m_visible = false;
    open = false;
  }

  // Handle window close
  if (!open && !m_completed) {
    // Window was closed (X button) - treat as cancel
    m_dialogResult.cancelled = true;
    m_completed = true;
    m_visible = false;
  }

  m_ImGui_End(m_ctx);
}
