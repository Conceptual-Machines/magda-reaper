#include "magda_imgui_plugin_window.h"
#include "magda_drum_mapping.h"
#include "magda_drum_mapping_window.h"
#include "magda_param_mapping.h"
#include "magda_param_mapping_window.h"
#include "magda_plugin_scanner.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// Theme colors (matching chat window)
namespace PluginTheme {
constexpr int WindowBg = 0x2D2D2DFF;
constexpr int ChildBg = 0x1A1A1AFF;
constexpr int Text = 0xE0E0E0FF;
constexpr int HeaderText = 0x88FF88FF;
constexpr int ButtonBg = 0x4A4A4AFF;
constexpr int TableRowAlt = 0x333333FF;
constexpr int DrumHighlight = 0x88AAFFFF;
} // namespace PluginTheme

// Helper macro for loading ImGui functions
#define LOAD_IMGUI_FUNC(name, type)                                            \
  m_##name = (type)rec->GetFunc(#name);                                        \
  if (!m_##name)                                                               \
    return false;

MagdaImGuiPluginWindow::MagdaImGuiPluginWindow() {
  m_ImGui_CreateContext = nullptr;
  m_ImGui_Begin = nullptr;
  m_ImGui_End = nullptr;
}

MagdaImGuiPluginWindow::~MagdaImGuiPluginWindow() { m_ctx = nullptr; }

bool MagdaImGuiPluginWindow::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load required ImGui functions
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
  LOAD_IMGUI_FUNC(ImGui_Selectable, bool (*)(void *, const char *, bool *,
                                             int *, double *, double *));

  m_available = true;
  return true;
}

void MagdaImGuiPluginWindow::Show() {
  m_visible = true;
  m_needsRefresh = true;
}

void MagdaImGuiPluginWindow::Hide() { m_visible = false; }

void MagdaImGuiPluginWindow::Toggle() {
  m_visible = !m_visible;
  if (m_visible) {
    m_needsRefresh = true;
  }
}

void MagdaImGuiPluginWindow::RefreshPluginList() {
  m_filteredPlugins.clear();

  if (!m_pluginScanner)
    return;

  std::string searchLower;
  if (m_searchBuffer[0]) {
    searchLower = m_searchBuffer;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                   ::tolower);
  }

  // Get deduplicated plugins
  auto plugins = m_pluginScanner->DeduplicatePlugins();
  const auto &aliasesByPlugin = m_pluginScanner->GetAliasesByPlugin();

  for (const auto &plugin : plugins) {
    const std::string &plugin_key =
        plugin.ident.empty() ? plugin.full_name : plugin.ident;

    // Build display name
    std::string displayName =
        plugin.name.empty() ? plugin.full_name : plugin.name;
    if (!plugin.manufacturer.empty()) {
      displayName += " (" + plugin.manufacturer + ")";
    }

    // Get alias
    std::string alias;
    auto it = aliasesByPlugin.find(plugin_key);
    if (it != aliasesByPlugin.end() && !it->second.empty()) {
      // Find shortest alias without bitness markers
      for (const auto &a : it->second) {
        std::string aLower = a;
        std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
        if (aLower.find("x64") == std::string::npos &&
            aLower.find("x86") == std::string::npos) {
          if (alias.empty() || a.length() < alias.length()) {
            alias = a;
          }
        }
      }
      if (alias.empty() && !it->second.empty()) {
        alias = it->second[0];
      }
    }

    // Apply search filter
    if (!searchLower.empty()) {
      std::string nameLower = displayName;
      std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                     ::tolower);
      std::string aliasLower = alias;
      std::transform(aliasLower.begin(), aliasLower.end(), aliasLower.begin(),
                     ::tolower);

      if (nameLower.find(searchLower) == std::string::npos &&
          aliasLower.find(searchLower) == std::string::npos) {
        continue;
      }
    }

    PluginRow row;
    row.name = displayName;
    row.alias = alias;
    row.plugin_key = plugin_key;
    row.is_instrument = plugin.is_instrument;
    // Check if this plugin has a drum mapping defined
    row.has_drum_mapping =
        (g_drumMappingManager &&
         g_drumMappingManager->GetMappingForPlugin(plugin_key) != nullptr);
    // Count param mappings
    row.param_mapping_count = 0;
    if (g_paramMappingManager) {
      const ParamMapping *pm =
          g_paramMappingManager->GetMappingForPlugin(plugin_key);
      if (pm) {
        row.param_mapping_count = (int)pm->aliases.size();
      }
    }
    m_filteredPlugins.push_back(row);
  }

  m_needsRefresh = false;
}

void MagdaImGuiPluginWindow::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    int flags = 0;
    m_ctx = m_ImGui_CreateContext("Plugins", &flags);
  }

  if (!m_ctx)
    return;

  // Refresh if needed
  if (m_needsRefresh) {
    RefreshPluginList();
  }

  // Set window size
  int condOnce = 2; // ImGuiCond_Once
  m_ImGui_SetNextWindowSize(m_ctx, 950, 600, &condOnce);

  bool open = true;
  int windowFlags = 0;

  // Push theme colors
  m_ImGui_PushStyleColor(m_ctx, 2, PluginTheme::WindowBg);  // Col_WindowBg
  m_ImGui_PushStyleColor(m_ctx, 3, PluginTheme::ChildBg);   // Col_ChildBg
  m_ImGui_PushStyleColor(m_ctx, 0, PluginTheme::Text);      // Col_Text
  m_ImGui_PushStyleColor(m_ctx, 21, PluginTheme::ButtonBg); // Col_Button

  if (m_ImGui_Begin(m_ctx, "Plugin Aliases", &open, &windowFlags)) {
    RenderHeader();
    m_ImGui_Separator(m_ctx);
    RenderPluginTable();
  }
  m_ImGui_End(m_ctx);

  // Pop theme colors
  int popCount = 4;
  m_ImGui_PopStyleColor(m_ctx, &popCount);

  // Handle window close
  if (!open) {
    m_visible = false;
    m_ctx = nullptr;
  }
}

void MagdaImGuiPluginWindow::RenderHeader() {
  m_ImGui_TextColored(m_ctx, PluginTheme::HeaderText, "Plugin Aliases");

  // Search box
  m_ImGui_Text(m_ctx, "Search:");
  double spacing = 10;
  m_ImGui_SameLine(m_ctx, nullptr, &spacing);

  char oldSearch[256];
  strncpy(oldSearch, m_searchBuffer, sizeof(oldSearch));

  if (m_ImGui_InputText(m_ctx, "##search", m_searchBuffer,
                        sizeof(m_searchBuffer), nullptr, nullptr)) {
    // Search changed - refresh
    if (strcmp(oldSearch, m_searchBuffer) != 0) {
      m_needsRefresh = true;
    }
  }

  m_ImGui_SameLine(m_ctx, nullptr, &spacing);

  if (m_ImGui_Button(m_ctx, "Refresh", nullptr, nullptr)) {
    if (m_pluginScanner) {
      m_pluginScanner->GenerateAliases();
    }
    m_needsRefresh = true;
  }

  // Stats
  char stats[128];
  snprintf(stats, sizeof(stats), "Showing %d plugins",
           (int)m_filteredPlugins.size());
  m_ImGui_SameLine(m_ctx, nullptr, &spacing);
  m_ImGui_Text(m_ctx, stats);
}

void MagdaImGuiPluginWindow::RenderPluginTable() {
  // Table flags: Resizable | BordersInnerV | RowBg | ScrollY
  int tableFlags = (1 << 1) | (1 << 8) | (1 << 6) | (1 << 12);

  if (m_ImGui_BeginTable(m_ctx, "##plugins", 6, &tableFlags, nullptr, nullptr,
                         nullptr)) {
    // Setup columns
    int colFlagsStretch = 1 << 3; // WidthStretch
    int colFlagsFixed = 1 << 4;   // WidthFixed
    double colNameWidth = 0.35;   // 35% for name
    double colTypeWidth = 75;     // Fixed for type
    double colAliasWidth = 0.20;  // 20% for alias
    double colActionsWidth = 100; // Fixed for Edit/Save/Cancel
    double colParamsWidth = 55;   // Fixed for params
    double colDrumsWidth = 55;    // Fixed for drums

    m_ImGui_TableSetupColumn(m_ctx, "Plugin Name", &colFlagsStretch,
                             &colNameWidth, nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "Type", &colFlagsFixed, &colTypeWidth,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "Alias", &colFlagsStretch, &colAliasWidth,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "Actions", &colFlagsFixed, &colActionsWidth,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "Params", &colFlagsFixed, &colParamsWidth,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "Drums", &colFlagsFixed, &colDrumsWidth,
                             nullptr);
    m_ImGui_TableHeadersRow(m_ctx);

    for (size_t i = 0; i < m_filteredPlugins.size(); i++) {
      auto &row = m_filteredPlugins[i];
      bool isEditing = (m_editingRowIndex == (int)i);

      m_ImGui_TableNextRow(m_ctx, nullptr, nullptr);

      // Column 1: Plugin name
      m_ImGui_TableNextColumn(m_ctx);
      m_ImGui_Text(m_ctx, row.name.c_str());

      // Column 2: Type (Instrument/Effect)
      m_ImGui_TableNextColumn(m_ctx);
      if (row.is_instrument) {
        m_ImGui_TextColored(m_ctx, 0x88CCFFFF, "Instrument"); // Light blue
      } else {
        m_ImGui_TextColored(m_ctx, 0xFFAA88FF, "Effect"); // Orange/peach
      }

      // Column 3: Alias (text or input depending on edit mode)
      m_ImGui_TableNextColumn(m_ctx);
      if (isEditing) {
        std::string aliasId = "##alias_" + std::to_string(i);
        m_ImGui_InputText(m_ctx, aliasId.c_str(), m_editAliasBuffer,
                          sizeof(m_editAliasBuffer), nullptr, nullptr);
      } else {
        std::string aliasDisplay = row.alias.empty() ? "-" : row.alias;
        m_ImGui_Text(m_ctx, aliasDisplay.c_str());
      }

      // Column 4: Actions (Edit or Save/Cancel)
      m_ImGui_TableNextColumn(m_ctx);
      if (isEditing) {
        // Save button
        std::string saveId = "Save##save_" + std::to_string(i);
        if (m_ImGui_Button(m_ctx, saveId.c_str(), nullptr, nullptr)) {
          // Save the alias
          row.alias = m_editAliasBuffer;
          if (m_pluginScanner) {
            m_pluginScanner->SetAliasForPlugin(row.plugin_key,
                                               m_editAliasBuffer);
            m_pluginScanner->SaveToCache();
          }
          m_editingRowIndex = -1; // Exit edit mode
        }
        double spacing = 5;
        m_ImGui_SameLine(m_ctx, nullptr, &spacing);
        // Cancel button
        std::string cancelId = "X##cancel_" + std::to_string(i);
        if (m_ImGui_Button(m_ctx, cancelId.c_str(), nullptr, nullptr)) {
          m_editingRowIndex = -1; // Exit edit mode without saving
        }
      } else {
        // Edit button
        std::string editId = "Edit##edit_" + std::to_string(i);
        if (m_ImGui_Button(m_ctx, editId.c_str(), nullptr, nullptr)) {
          m_editingRowIndex = (int)i;
          strncpy(m_editAliasBuffer, row.alias.c_str(),
                  sizeof(m_editAliasBuffer) - 1);
          m_editAliasBuffer[sizeof(m_editAliasBuffer) - 1] = '\0';
        }
      }

      // Column 5: Params (clickable to open param mapping)
      m_ImGui_TableNextColumn(m_ctx);
      std::string paramsDisplay = row.param_mapping_count > 0
                                      ? std::to_string(row.param_mapping_count)
                                      : "-";
      std::string paramsId = paramsDisplay + "##params_" + std::to_string(i);
      bool paramsSelected = false;
      if (row.param_mapping_count > 0) {
        m_ImGui_PushStyleColor(m_ctx, 0, 0x88FF88FF); // Green text
      }
      if (m_ImGui_Selectable(m_ctx, paramsId.c_str(), &paramsSelected, nullptr,
                             nullptr, nullptr)) {
        if (g_paramMappingWindow && g_paramMappingWindow->IsAvailable()) {
          g_paramMappingWindow->Show(row.plugin_key, row.name);
        }
      }
      if (row.param_mapping_count > 0) {
        int one = 1;
        m_ImGui_PopStyleColor(m_ctx, &one);
      }

      // Column 6: Drums (clickable to open drum mapping - only for instruments)
      m_ImGui_TableNextColumn(m_ctx);
      if (row.is_instrument) {
        std::string drumsDisplay = row.has_drum_mapping ? "✓" : "-";
        std::string drumsId = drumsDisplay + "##drums_" + std::to_string(i);
        bool drumsSelected = false;
        if (row.has_drum_mapping) {
          m_ImGui_PushStyleColor(m_ctx, 0, 0x88FF88FF); // Green text
        }
        if (m_ImGui_Selectable(m_ctx, drumsId.c_str(), &drumsSelected, nullptr,
                               nullptr, nullptr)) {
          if (g_drumMappingWindow && g_drumMappingWindow->IsAvailable()) {
            g_drumMappingWindow->Show(row.plugin_key, row.name);
          }
        }
        if (row.has_drum_mapping) {
          int one = 1;
          m_ImGui_PopStyleColor(m_ctx, &one);
        }
      } else {
        m_ImGui_Text(m_ctx, "-");
      }
    }

    m_ImGui_EndTable(m_ctx);
  }
}
