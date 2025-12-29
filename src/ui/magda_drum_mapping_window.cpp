#include "magda_drum_mapping_window.h"
#include <cstdio>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

// Global instance
MagdaDrumMappingWindow *g_drumMappingWindow = nullptr;

// Theme colors
namespace DrumTheme {
constexpr int WindowBg = 0x2D2D2DFF;
constexpr int ChildBg = 0x1A1A1AFF;
constexpr int Text = 0xE0E0E0FF;
constexpr int HeaderText = 0x88FF88FF;
constexpr int ButtonBg = 0x4A4A4AFF;
} // namespace DrumTheme

// Helper macro
#define LOAD_IMGUI_FUNC(name, type)                                            \
  m_##name = (type)rec->GetFunc(#name);                                        \
  if (!m_##name)                                                               \
    return false;

MagdaDrumMappingWindow::MagdaDrumMappingWindow() {}

MagdaDrumMappingWindow::~MagdaDrumMappingWindow() { m_ctx = nullptr; }

bool MagdaDrumMappingWindow::Initialize(reaper_plugin_info_t *rec) {
  if (!rec)
    return false;

  // Load EXACTLY the same functions as param window (which works)
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

void MagdaDrumMappingWindow::Show(const std::string &plugin_key,
                                  const std::string &plugin_name) {
  m_pluginKey = plugin_key;
  m_pluginName = plugin_name;
  m_visible = true;
  m_hasChanges = false;

  LoadMappingForPlugin();
}

void MagdaDrumMappingWindow::Hide() { m_visible = false; }

void MagdaDrumMappingWindow::LoadMappingForPlugin() {
  if (!g_drumMappingManager) {
    g_drumMappingManager = new DrumMappingManager();
  }

  const DrumMapping *existing =
      g_drumMappingManager->GetMappingForPlugin(m_pluginKey);
  if (existing) {
    m_editingMapping = *existing;
  } else {
    // Create new from General MIDI preset
    m_editingMapping = g_drumMappingManager->CreateMappingFromPreset(
        "general_midi", m_pluginKey, m_pluginName);
  }
}

void MagdaDrumMappingWindow::SaveMapping() {
  if (!g_drumMappingManager)
    return;

  g_drumMappingManager->SetMapping(m_editingMapping);
  m_hasChanges = false;

  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Drum mapping saved\n");
    }
  }
}

void MagdaDrumMappingWindow::ApplyPreset(const std::string &preset_id) {
  auto presets = DrumMappingManager::GetPresetMappings();
  for (const auto &preset : presets) {
    if (preset.id == preset_id) {
      m_editingMapping.notes = preset.notes;
      m_hasChanges = true;
      break;
    }
  }
}

void MagdaDrumMappingWindow::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    int flags = 0;
    m_ctx = m_ImGui_CreateContext("Drum Mapping", &flags);
  }

  if (!m_ctx)
    return;

  // Set window size
  int condOnce = 2;
  m_ImGui_SetNextWindowSize(m_ctx, 500, 500, &condOnce);

  bool open = true;
  int windowFlags = 0;

  // Theme
  m_ImGui_PushStyleColor(m_ctx, 2, DrumTheme::WindowBg);
  m_ImGui_PushStyleColor(m_ctx, 3, DrumTheme::ChildBg);
  m_ImGui_PushStyleColor(m_ctx, 0, DrumTheme::Text);
  m_ImGui_PushStyleColor(m_ctx, 21, DrumTheme::ButtonBg);

  // Extract display name from path
  std::string displayName = m_pluginName;
  size_t lastSlash = displayName.rfind('/');
  if (lastSlash != std::string::npos) {
    displayName = displayName.substr(lastSlash + 1);
  }
  size_t lastDot = displayName.rfind('.');
  if (lastDot != std::string::npos) {
    displayName = displayName.substr(0, lastDot);
  }

  std::string title = "Drum Mapping: " + displayName;
  if (m_hasChanges) {
    title += " *";
  }

  if (m_ImGui_Begin(m_ctx, title.c_str(), &open, &windowFlags)) {
    // Header
    m_ImGui_TextColored(m_ctx, DrumTheme::HeaderText,
                        "Map canonical drum names to MIDI notes");
    m_ImGui_Separator(m_ctx);

    // Get drum names
    auto drumNames = CanonicalDrums::GetAllDrumNames();

    // Table
    int tableFlags = (1 << 1) | (1 << 8) | (1 << 6) | (1 << 12);
    if (m_ImGui_BeginTable(m_ctx, "##drums", 2, &tableFlags, nullptr, nullptr,
                           nullptr)) {
      int colFlagsStretch = 1 << 3;
      double col0Width = 0.6;
      double col1Width = 0.4;

      m_ImGui_TableSetupColumn(m_ctx, "Drum Name", &colFlagsStretch, &col0Width,
                               nullptr);
      m_ImGui_TableSetupColumn(m_ctx, "MIDI Note", &colFlagsStretch, &col1Width,
                               nullptr);
      m_ImGui_TableHeadersRow(m_ctx);

      for (size_t i = 0; i < drumNames.size(); i++) {
        const std::string &drumName = drumNames[i];

        m_ImGui_TableNextRow(m_ctx, nullptr, nullptr);

        // Drum name
        m_ImGui_TableNextColumn(m_ctx);
        m_ImGui_Text(m_ctx, drumName.c_str());

        // MIDI note input
        m_ImGui_TableNextColumn(m_ctx);
        int note = m_editingMapping.GetNote(drumName);
        if (note < 0)
          note = 0;

        char noteBuf[16];
        snprintf(noteBuf, sizeof(noteBuf), "%d", note);

        std::string inputId = "##note_" + std::to_string(i);
        if (m_ImGui_InputText(m_ctx, inputId.c_str(), noteBuf, sizeof(noteBuf),
                              nullptr, nullptr)) {
          int newNote = atoi(noteBuf);
          if (newNote >= 0 && newNote <= 127) {
            m_editingMapping.SetNote(drumName, newNote);
            m_hasChanges = true;
          }
        }
      }

      m_ImGui_EndTable(m_ctx);
    }

    m_ImGui_Separator(m_ctx);

    // Buttons
    double spacing = 10;
    if (m_ImGui_Button(m_ctx, "Save", nullptr, nullptr)) {
      SaveMapping();
    }

    m_ImGui_SameLine(m_ctx, nullptr, &spacing);

    if (m_ImGui_Button(m_ctx, "Reset", nullptr, nullptr)) {
      LoadMappingForPlugin();
      m_hasChanges = false;
    }

    m_ImGui_SameLine(m_ctx, nullptr, &spacing);

    if (m_ImGui_Button(m_ctx, "Close", nullptr, nullptr)) {
      Hide();
    }

    if (m_hasChanges) {
      m_ImGui_SameLine(m_ctx, nullptr, &spacing);
      m_ImGui_TextColored(m_ctx, 0xFFFF88FF, "(unsaved changes)");
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
