#include "magda_param_mapping_window.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// Global instance - defined in main.cpp
// MagdaParamMappingWindow *g_paramMappingWindow = nullptr;

// Theme colors
namespace ParamTheme {
constexpr int WindowBg = 0x2D2D2DFF;
constexpr int ChildBg = 0x1A1A1AFF;
constexpr int Text = 0xE0E0E0FF;
constexpr int HeaderText = 0x88FF88FF;
constexpr int ButtonBg = 0x4A4A4AFF;
constexpr int AliasText = 0x88CCFFFF;
} // namespace ParamTheme

// Helper macro
#define LOAD_IMGUI_FUNC(name, type)                                            \
  m_##name = (type)rec->GetFunc(#name);                                        \
  if (!m_##name)                                                               \
    return false;

MagdaParamMappingWindow::MagdaParamMappingWindow() {}

MagdaParamMappingWindow::~MagdaParamMappingWindow() { m_ctx = nullptr; }

bool MagdaParamMappingWindow::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  LOAD_IMGUI_FUNC(ImGui_CreateContext, void *(*)(const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_Begin, bool (*)(void *, const char *, bool *, int *));
  LOAD_IMGUI_FUNC(ImGui_End, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetNextWindowSize,
                  void (*)(void *, double, double, int *));
  LOAD_IMGUI_FUNC(ImGui_Text, void (*)(void *, const char *));
  LOAD_IMGUI_FUNC(ImGui_TextColored, void (*)(void *, int, const char *));
  LOAD_IMGUI_FUNC(ImGui_InputText,
                  bool (*)(void *, const char *, char *, int, int *, void *));
  LOAD_IMGUI_FUNC(ImGui_Button,
                  bool (*)(void *, const char *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_SameLine, void (*)(void *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_Separator, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_BeginChild, bool (*)(void *, const char *, double *,
                                             double *, int *, int *));
  LOAD_IMGUI_FUNC(ImGui_EndChild, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_PushStyleColor, void (*)(void *, int, int));
  LOAD_IMGUI_FUNC(ImGui_PopStyleColor, void (*)(void *, int *));
  LOAD_IMGUI_FUNC(ImGui_BeginTable, bool (*)(void *, const char *, int, int *,
                                             double *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_EndTable, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_TableNextRow, void (*)(void *, int *, double *));
  LOAD_IMGUI_FUNC(ImGui_TableNextColumn, bool (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_TableSetupColumn,
                  void (*)(void *, const char *, int *, double *, int *));
  LOAD_IMGUI_FUNC(ImGui_TableHeadersRow, void (*)(void *));

  m_available = true;
  return true;
}

void MagdaParamMappingWindow::Show(const std::string &plugin_key,
                                   const std::string &plugin_name) {
  m_pluginKey = plugin_key;
  m_pluginName = plugin_name;
  m_visible = true;
  m_hasChanges = false;
  m_searchBuffer[0] = '\0';

  LoadPluginParams();
  LoadExistingAliases();
}

void MagdaParamMappingWindow::Hide() { m_visible = false; }

void MagdaParamMappingWindow::LoadPluginParams() {
  m_params.clear();

  if (!g_rec)
    return;

  // Get Reaper API functions
  int (*CountTracks)(ReaProject *) =
      (int (*)(ReaProject *))g_rec->GetFunc("CountTracks");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
  int (*TrackFX_GetCount)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("TrackFX_GetCount");
  bool (*TrackFX_GetFXName)(MediaTrack *, int, char *, int) = (bool (*)(
      MediaTrack *, int, char *, int))g_rec->GetFunc("TrackFX_GetFXName");
  int (*TrackFX_GetNumParams)(MediaTrack *, int) =
      (int (*)(MediaTrack *, int))g_rec->GetFunc("TrackFX_GetNumParams");
  bool (*TrackFX_GetParamName)(MediaTrack *, int, int, char *, int) =
      (bool (*)(MediaTrack *, int, int, char *, int))g_rec->GetFunc(
          "TrackFX_GetParamName");

  if (!CountTracks || !GetTrack || !TrackFX_GetCount || !TrackFX_GetFXName ||
      !TrackFX_GetNumParams || !TrackFX_GetParamName) {
    return;
  }

  // Find the first instance of this plugin on any track
  int trackCount = CountTracks(nullptr);
  for (int t = 0; t < trackCount; t++) {
    MediaTrack *track = GetTrack(nullptr, t);
    if (!track)
      continue;

    int fxCount = TrackFX_GetCount(track);
    for (int fx = 0; fx < fxCount; fx++) {
      char fxName[512] = {0};
      TrackFX_GetFXName(track, fx, fxName, sizeof(fxName));

      // Check if this is our plugin
      if (m_pluginKey == fxName) {
        // Found it! Get all parameters
        int numParams = TrackFX_GetNumParams(track, fx);
        for (int p = 0; p < numParams; p++) {
          char paramName[256] = {0};
          TrackFX_GetParamName(track, fx, p, paramName, sizeof(paramName));

          PluginParam param;
          param.index = p;
          param.name = paramName;
          param.currentAlias = "";
          m_params.push_back(param);
        }

        // Done - found and loaded params
        return;
      }
    }
  }

  // Plugin not found on any track - show message in console
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Plugin not found on any track. Add the plugin to a "
                   "track first to see its parameters.\n");
  }
}

void MagdaParamMappingWindow::LoadExistingAliases() {
  if (!g_paramMappingManager)
    return;

  const ParamMapping *mapping =
      g_paramMappingManager->GetMappingForPlugin(m_pluginKey);
  if (!mapping)
    return;

  // Apply existing aliases to params
  for (auto &param : m_params) {
    // Find alias for this param index
    for (const auto &alias : mapping->aliases) {
      if (alias.second == param.index) {
        param.currentAlias = alias.first;
        break;
      }
    }
  }
}

void MagdaParamMappingWindow::SaveMapping() {
  if (!g_paramMappingManager)
    return;

  ParamMapping mapping;
  mapping.plugin_key = m_pluginKey;
  mapping.plugin_name = m_pluginName;

  for (const auto &param : m_params) {
    if (!param.currentAlias.empty()) {
      mapping.aliases[param.currentAlias] = param.index;
    }
  }

  g_paramMappingManager->SetMapping(mapping);
  m_hasChanges = false;

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Parameter mappings saved\n");
  }
}

void MagdaParamMappingWindow::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    int flags = 0;
    m_ctx = m_ImGui_CreateContext("Param Mapping", &flags);
  }

  if (!m_ctx)
    return;

  // Set window size
  int condOnce = 2;
  m_ImGui_SetNextWindowSize(m_ctx, 700, 500, &condOnce);

  bool open = true;
  int windowFlags = 0;

  // Theme
  m_ImGui_PushStyleColor(m_ctx, 2, ParamTheme::WindowBg);
  m_ImGui_PushStyleColor(m_ctx, 3, ParamTheme::ChildBg);
  m_ImGui_PushStyleColor(m_ctx, 0, ParamTheme::Text);
  m_ImGui_PushStyleColor(m_ctx, 21, ParamTheme::ButtonBg);

  std::string title = "Parameter Mapping: " + m_pluginName;
  if (m_hasChanges) {
    title += " *";
  }

  if (m_ImGui_Begin(m_ctx, title.c_str(), &open, &windowFlags)) {
    // Header
    m_ImGui_TextColored(m_ctx, ParamTheme::HeaderText,
                        "Assign aliases to plugin parameters");
    m_ImGui_Text(m_ctx, "Use canonical names like: cutoff, resonance, attack, "
                        "decay, mix, etc.");
    m_ImGui_Separator(m_ctx);

    // Search
    m_ImGui_Text(m_ctx, "Filter:");
    double spacing = 10;
    m_ImGui_SameLine(m_ctx, nullptr, &spacing);
    m_ImGui_InputText(m_ctx, "##filter", m_searchBuffer, sizeof(m_searchBuffer),
                      nullptr, nullptr);

    m_ImGui_Separator(m_ctx);

    // Check if we have params
    if (m_params.empty()) {
      m_ImGui_TextColored(m_ctx, 0xFFAAAAFF,
                          "No parameters found. Make sure the plugin is added "
                          "to a track in your project.");
    } else {
      // Table
      int tableFlags = (1 << 1) | (1 << 8) | (1 << 6) | (1 << 12);
      if (m_ImGui_BeginTable(m_ctx, "##params", 3, &tableFlags, nullptr,
                             nullptr, nullptr)) {
        int colFlagsFixed = 1 << 4;
        int colFlagsStretch = 1 << 3;
        double col0Width = 60;
        double col1Width = 0.5;
        double col2Width = 0.3;

        m_ImGui_TableSetupColumn(m_ctx, "Index", &colFlagsFixed, &col0Width,
                                 nullptr);
        m_ImGui_TableSetupColumn(m_ctx, "Parameter Name", &colFlagsStretch,
                                 &col1Width, nullptr);
        m_ImGui_TableSetupColumn(m_ctx, "Alias", &colFlagsStretch, &col2Width,
                                 nullptr);
        m_ImGui_TableHeadersRow(m_ctx);

        std::string filterLower;
        if (m_searchBuffer[0]) {
          filterLower = m_searchBuffer;
          std::transform(filterLower.begin(), filterLower.end(),
                         filterLower.begin(), ::tolower);
        }

        for (size_t i = 0; i < m_params.size(); i++) {
          auto &param = m_params[i];

          // Apply filter
          if (!filterLower.empty()) {
            std::string nameLower = param.name;
            std::transform(nameLower.begin(), nameLower.end(),
                           nameLower.begin(), ::tolower);
            std::string aliasLower = param.currentAlias;
            std::transform(aliasLower.begin(), aliasLower.end(),
                           aliasLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos &&
                aliasLower.find(filterLower) == std::string::npos) {
              continue;
            }
          }

          m_ImGui_TableNextRow(m_ctx, nullptr, nullptr);

          // Index
          m_ImGui_TableNextColumn(m_ctx);
          char indexStr[16];
          snprintf(indexStr, sizeof(indexStr), "%d", param.index);
          m_ImGui_Text(m_ctx, indexStr);

          // Parameter name
          m_ImGui_TableNextColumn(m_ctx);
          m_ImGui_Text(m_ctx, param.name.c_str());

          // Alias input
          m_ImGui_TableNextColumn(m_ctx);
          char aliasBuf[128];
          strncpy(aliasBuf, param.currentAlias.c_str(), sizeof(aliasBuf) - 1);
          aliasBuf[sizeof(aliasBuf) - 1] = '\0';

          std::string inputId = "##alias_" + std::to_string(i);
          if (m_ImGui_InputText(m_ctx, inputId.c_str(), aliasBuf,
                                sizeof(aliasBuf), nullptr, nullptr)) {
            if (param.currentAlias != aliasBuf) {
              param.currentAlias = aliasBuf;
              m_hasChanges = true;
            }
          }
        }

        m_ImGui_EndTable(m_ctx);
      }
    }

    m_ImGui_Separator(m_ctx);

    // Buttons
    if (m_ImGui_Button(m_ctx, "Save", nullptr, nullptr)) {
      SaveMapping();
    }

    m_ImGui_SameLine(m_ctx, nullptr, &spacing);

    if (m_ImGui_Button(m_ctx, "Refresh Params", nullptr, nullptr)) {
      LoadPluginParams();
      LoadExistingAliases();
    }

    m_ImGui_SameLine(m_ctx, nullptr, &spacing);

    if (m_ImGui_Button(m_ctx, "Close", nullptr, nullptr)) {
      Hide();
    }
  }
  m_ImGui_End(m_ctx);

  int popCount = 4;
  m_ImGui_PopStyleColor(m_ctx, &popCount);

  if (!open) {
    m_visible = false;
    m_ctx = nullptr;
  }
}
