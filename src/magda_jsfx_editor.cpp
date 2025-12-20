#include "magda_jsfx_editor.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#define REAPER_PLUGIN_DLL_EXPORT
#include "reaper_plugin.h"

MagdaJSFXEditor *g_jsfxEditor = nullptr;

// Theme colors (ABGR format for ImGui)
#define THEME_RGBA(r, g, b) (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))
struct ThemeColors {
  int headerText = THEME_RGBA(0xE0, 0xE0, 0xE0);   // White headers
  int normalText = THEME_RGBA(0xC0, 0xC0, 0xC0);   // Light grey text
  int dimText = THEME_RGBA(0x90, 0x90, 0x90);      // Dimmed grey
  int windowBg = THEME_RGBA(0x1A, 0x1A, 0x1A);     // Dark background
  int childBg = THEME_RGBA(0x25, 0x25, 0x25);      // Slightly lighter
  int inputBg = THEME_RGBA(0x30, 0x30, 0x30);      // Input field background
  int buttonBg = THEME_RGBA(0x40, 0x40, 0x40);     // Button background
};
static ThemeColors g_theme;

// ImGui flags
namespace ImGuiWindowFlags {
constexpr int None = 0;
constexpr int NoCollapse = 32;
constexpr int MenuBar = 1024;
constexpr int AlwaysVerticalScrollbar = 16384;
constexpr int AlwaysHorizontalScrollbar = 32768;
} // namespace ImGuiWindowFlags

namespace ImGuiInputTextFlags {
constexpr int None = 0;
constexpr int AllowTabInput = 1024;
} // namespace ImGuiInputTextFlags

namespace ImGuiTableFlags {
constexpr int Resizable = 1;
constexpr int BordersInnerV = 128;
} // namespace ImGuiTableFlags

namespace ImGuiTableColumnFlags {
constexpr int WidthFixed = 16;
constexpr int WidthStretch = 32;
} // namespace ImGuiTableColumnFlags

namespace ImGuiCol {
constexpr int Text = 0;
constexpr int ChildBg = 7;
constexpr int FrameBg = 10;
} // namespace ImGuiCol

MagdaJSFXEditor::MagdaJSFXEditor() {
  memset(m_editorBuffer, 0, sizeof(m_editorBuffer));
  memset(m_chatInput, 0, sizeof(m_chatInput));
}

MagdaJSFXEditor::~MagdaJSFXEditor() {
  if (m_ctx && m_ImGui_DestroyContext) {
    m_ImGui_DestroyContext(m_ctx);
  }
}

bool MagdaJSFXEditor::Initialize(reaper_plugin_info_t *rec) {
  m_rec = rec;

  // Load ReaImGui functions
  m_ImGui_CreateContext =
      (void *(*)(const char *, int *))rec->GetFunc("ImGui_CreateContext");
  m_ImGui_DestroyContext =
      (void (*)(void *))rec->GetFunc("ImGui_DestroyContext");
  m_ImGui_Begin =
      (bool (*)(void *, const char *, bool *, int *))rec->GetFunc("ImGui_Begin");
  m_ImGui_End = (void (*)(void *))rec->GetFunc("ImGui_End");
  m_ImGui_Text = (void (*)(void *, const char *))rec->GetFunc("ImGui_Text");
  m_ImGui_TextWrapped =
      (void (*)(void *, const char *))rec->GetFunc("ImGui_TextWrapped");
  m_ImGui_TextColored =
      (void (*)(void *, int, const char *))rec->GetFunc("ImGui_TextColored");
  m_ImGui_Button = (bool (*)(void *, const char *, double *, double *))rec->GetFunc(
      "ImGui_Button");
  m_ImGui_Selectable =
      (bool (*)(void *, const char *, bool *, int *, double *, double *))rec->GetFunc(
          "ImGui_Selectable");
  m_ImGui_InputText =
      (bool (*)(void *, const char *, char *, int, int *, void *))rec->GetFunc(
          "ImGui_InputText");
  m_ImGui_InputTextMultiline =
      (bool (*)(void *, const char *, char *, int, double *, double *, int *,
                void *))rec->GetFunc("ImGui_InputTextMultiline");
  m_ImGui_Separator = (void (*)(void *))rec->GetFunc("ImGui_Separator");
  m_ImGui_SameLine =
      (void (*)(void *, double *, double *))rec->GetFunc("ImGui_SameLine");
  m_ImGui_Dummy =
      (void (*)(void *, double, double))rec->GetFunc("ImGui_Dummy");
  m_ImGui_BeginChild =
      (bool (*)(void *, const char *, double *, double *, int *, int *))rec->GetFunc(
          "ImGui_BeginChild");
  m_ImGui_EndChild = (void (*)(void *))rec->GetFunc("ImGui_EndChild");
  m_ImGui_SetNextWindowSize =
      (void (*)(void *, double, double, int *))rec->GetFunc(
          "ImGui_SetNextWindowSize");
  m_ImGui_PushStyleColor =
      (void (*)(void *, int, int))rec->GetFunc("ImGui_PushStyleColor");
  m_ImGui_PopStyleColor =
      (void (*)(void *, int *))rec->GetFunc("ImGui_PopStyleColor");
  m_ImGui_GetContentRegionAvail =
      (void (*)(void *, double *, double *))rec->GetFunc(
          "ImGui_GetContentRegionAvail");
  m_ImGui_GetTextLineHeight =
      (double (*)(void *))rec->GetFunc("ImGui_GetTextLineHeight");
  m_ImGui_BeginGroup = (void (*)(void *))rec->GetFunc("ImGui_BeginGroup");
  m_ImGui_EndGroup = (void (*)(void *))rec->GetFunc("ImGui_EndGroup");
  m_ImGui_BeginTable =
      (bool (*)(void *, const char *, int, int *, double *, double *, double *))rec->GetFunc(
          "ImGui_BeginTable");
  m_ImGui_EndTable = (void (*)(void *))rec->GetFunc("ImGui_EndTable");
  m_ImGui_TableNextRow =
      (void (*)(void *, int *, double *))rec->GetFunc("ImGui_TableNextRow");
  m_ImGui_TableNextColumn =
      (void (*)(void *))rec->GetFunc("ImGui_TableNextColumn");
  m_ImGui_TableSetupColumn =
      (void (*)(void *, const char *, int *, double *, double *))rec->GetFunc(
          "ImGui_TableSetupColumn");
  m_ImGui_GetStyleColor =
      (int (*)(void *, int))rec->GetFunc("ImGui_GetStyleColor");
  m_ImGui_SetCursorPosY =
      (void (*)(void *, double))rec->GetFunc("ImGui_SetCursorPosY");
  m_ImGui_GetCursorPosY = (double (*)(void *))rec->GetFunc("ImGui_GetCursorPosY");
  m_ImGui_GetScrollY = (double (*)(void *))rec->GetFunc("ImGui_GetScrollY");
  m_ImGui_SetScrollY = (void (*)(void *, double))rec->GetFunc("ImGui_SetScrollY");
  m_ImGui_GetScrollMaxY =
      (double (*)(void *))rec->GetFunc("ImGui_GetScrollMaxY");

  // Check if all required functions are available
  m_available = m_ImGui_CreateContext && m_ImGui_Begin && m_ImGui_End &&
                m_ImGui_Text && m_ImGui_Button && m_ImGui_InputTextMultiline &&
                m_ImGui_BeginChild && m_ImGui_EndChild;

  if (m_available) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: JSFX Editor initialized\n");
    }
  }

  // Initialize file browser with Effects folder
  m_currentFolder = GetEffectsFolder();
  RefreshFileList();

  return m_available;
}

std::string MagdaJSFXEditor::GetEffectsFolder() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    return std::string(path) + "\\REAPER\\Effects";
  }
  return "";
#elif __APPLE__
  const char *home = getenv("HOME");
  if (home) {
    return std::string(home) + "/Library/Application Support/REAPER/Effects";
  }
  return "";
#else
  const char *home = getenv("HOME");
  if (home) {
    return std::string(home) + "/.config/REAPER/Effects";
  }
  return "";
#endif
}

void MagdaJSFXEditor::RefreshFileList() {
  m_files.clear();

  DIR *dir = opendir(m_currentFolder.c_str());
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.')
      continue; // Skip hidden files

    JSFXFileEntry file;
    file.name = entry->d_name;
    file.full_path = m_currentFolder + "/" + entry->d_name;
    file.depth = 0;
    file.is_expanded = false;

    struct stat st;
    if (stat(file.full_path.c_str(), &st) == 0) {
      file.is_directory = S_ISDIR(st.st_mode);
    } else {
      file.is_directory = false;
    }

    // Only show JSFX files and directories
    if (file.is_directory || file.name.find(".jsfx") != std::string::npos) {
      m_files.push_back(file);
    }
  }
  closedir(dir);

  // Sort: directories first, then alphabetically
  std::sort(m_files.begin(), m_files.end(),
            [](const JSFXFileEntry &a, const JSFXFileEntry &b) {
              if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
              }
              return a.name < b.name;
            });
}

void MagdaJSFXEditor::OpenFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  strncpy(m_editorBuffer, content.c_str(), sizeof(m_editorBuffer) - 1);
  m_editorBuffer[sizeof(m_editorBuffer) - 1] = '\0';

  m_currentFilePath = path;
  // Extract filename from path
  size_t lastSlash = path.find_last_of("/\\");
  m_currentFileName =
      (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
  m_modified = false;

  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Opened %s\n",
             m_currentFileName.c_str());
    ShowConsoleMsg(msg);
  }
}

void MagdaJSFXEditor::SaveCurrentFile() {
  if (m_currentFilePath.empty()) {
    // TODO: SaveAs dialog
    return;
  }

  std::ofstream file(m_currentFilePath);
  if (file.is_open()) {
    file << m_editorBuffer;
    file.close();
    m_modified = false;

    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Saved %s\n",
               m_currentFileName.c_str());
      ShowConsoleMsg(msg);
    }
  }
}

void MagdaJSFXEditor::NewFile() {
  memset(m_editorBuffer, 0, sizeof(m_editorBuffer));
  m_currentFilePath = "";
  m_currentFileName = "untitled.jsfx";
  m_modified = false;

  // Template for new JSFX
  const char *template_code = R"(desc:My Effect

slider1:0<-60,0,1>Gain (dB)

@init
gain = 1;

@slider
gain = 10^(slider1/20);

@sample
spl0 *= gain;
spl1 *= gain;
)";

  strncpy(m_editorBuffer, template_code, sizeof(m_editorBuffer) - 1);
}

void MagdaJSFXEditor::Show() {
  if (!m_available)
    return;

  if (!m_ctx) {
    int configFlags = 0;
    m_ctx = m_ImGui_CreateContext("MAGDA JSFX Editor", &configFlags);
    if (!m_ctx) {
      // Context creation failed
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA JSFX: Failed to create ImGui context\n");
      }
      return;
    }
  }
  m_visible = true;
}

void MagdaJSFXEditor::Hide() { m_visible = false; }

void MagdaJSFXEditor::Render() {
  if (!m_visible || !m_ctx || !m_available)
    return;

  // Set initial window size
  int cond = 4; // ImGuiCond_FirstUseEver
  m_ImGui_SetNextWindowSize(m_ctx, 1200, 700, &cond);

  bool open = true;
  int windowFlags = ImGuiWindowFlags::MenuBar;

  if (m_ImGui_Begin(m_ctx, "MAGDA JSFX Editor###jsfx_editor", &open,
                    &windowFlags)) {
    // Get available space
    double availW, availH;
    m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);

    // Panel widths
    double filePanelW = 200;
    double chatPanelW = 300;
    double editorPanelW = availW - filePanelW - chatPanelW - 20;
    double panelH = availH - 40; // Leave room for toolbar

    // Three-panel layout using table
    int tableFlags = ImGuiTableFlags::Resizable | ImGuiTableFlags::BordersInnerV;
    double zero = 0;

    if (m_ImGui_BeginTable(m_ctx, "##layout", 3, &tableFlags, &zero, &zero, &zero)) {
      // Setup columns
      int fixedFlags = ImGuiTableColumnFlags::WidthFixed;
      int stretchFlags = ImGuiTableColumnFlags::WidthStretch;

      m_ImGui_TableSetupColumn(m_ctx, "Files", &fixedFlags, &filePanelW, &zero);
      m_ImGui_TableSetupColumn(m_ctx, "Editor", &stretchFlags, &zero, &zero);
      m_ImGui_TableSetupColumn(m_ctx, "Chat", &fixedFlags, &chatPanelW, &zero);

      m_ImGui_TableNextRow(m_ctx, nullptr, &panelH);

      // Files panel
      m_ImGui_TableNextColumn(m_ctx);
      RenderFilePanel();

      // Editor panel
      m_ImGui_TableNextColumn(m_ctx);
      RenderEditorPanel();

      // Chat panel
      m_ImGui_TableNextColumn(m_ctx);
      RenderChatPanel();

      m_ImGui_EndTable(m_ctx);
    }

    // Toolbar at bottom
    m_ImGui_Separator(m_ctx);
    RenderToolbar();
  }
  m_ImGui_End(m_ctx);

  if (!open) {
    m_visible = false;
  }
}

void MagdaJSFXEditor::RenderFilePanel() {
  m_ImGui_TextColored(m_ctx, g_theme.headerText, "FILES");
  m_ImGui_Separator(m_ctx);

  // File list in a scrollable child
  double childW = 0;  // Fill available width
  double childH = -1; // Fill available height
  int childFlags = 0;
  int windowFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

  if (m_ImGui_BeginChild(m_ctx, "##file_list", &childW, &childH, &childFlags,
                         &windowFlags)) {
    for (auto &file : m_files) {
      std::string icon = file.is_directory ? "📁 " : "📄 ";
      std::string label = icon + file.name;

      bool selected = (file.full_path == m_currentFilePath);
      int selectableFlags = 0;
      if (m_ImGui_Selectable(m_ctx, label.c_str(), &selected, &selectableFlags, nullptr,
                             nullptr)) {
        if (file.is_directory) {
          // Navigate into directory
          m_currentFolder = file.full_path;
          RefreshFileList();
        } else {
          // Open file
          OpenFile(file.full_path);
        }
      }
    }
  }
  m_ImGui_EndChild(m_ctx);
}

void MagdaJSFXEditor::RenderEditorPanel() {
  // Header with filename
  std::string header = m_currentFileName;
  if (m_modified) {
    header += " *";
  }
  m_ImGui_TextColored(m_ctx, g_theme.headerText, header.c_str());
  m_ImGui_Separator(m_ctx);

  // Code editor
  double editorW = -1; // Fill width
  double editorH = -1; // Fill height
  int inputFlags = ImGuiInputTextFlags::AllowTabInput;

  // Dark background for code
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBg, 0xFF1E1E1E);

  if (m_ImGui_InputTextMultiline(m_ctx, "##code_editor", m_editorBuffer,
                                  sizeof(m_editorBuffer), &editorW, &editorH,
                                  &inputFlags, nullptr)) {
    m_modified = true;
  }

  int one = 1;
  m_ImGui_PopStyleColor(m_ctx, &one);
}

void MagdaJSFXEditor::RenderChatPanel() {
  m_ImGui_TextColored(m_ctx, g_theme.headerText, "AI ASSISTANT");
  m_ImGui_Separator(m_ctx);

  // Chat history
  double chatW = 0;
  double chatH = -60; // Leave room for input
  int childFlags = 1; // Border
  int windowFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

  if (m_ImGui_BeginChild(m_ctx, "##chat_history", &chatW, &chatH, &childFlags,
                         &windowFlags)) {
    if (m_chatHistory.empty()) {
      m_ImGui_TextColored(m_ctx, g_theme.dimText,
                          "Ask me to help write or modify your JSFX code!");
      m_ImGui_Dummy(m_ctx, 0, 10);
      m_ImGui_TextColored(m_ctx, g_theme.dimText, "Examples:");
      m_ImGui_TextColored(m_ctx, g_theme.dimText, "• Create a soft clipper");
      m_ImGui_TextColored(m_ctx, g_theme.dimText, "• Add a wet/dry mix control");
      m_ImGui_TextColored(m_ctx, g_theme.dimText, "• Explain this code");
    }

    for (const auto &msg : m_chatHistory) {
      if (msg.is_user) {
        m_ImGui_TextColored(m_ctx, 0xFF88CCFF, "You:");
      } else {
        m_ImGui_TextColored(m_ctx, 0xFF88FF88, "AI:");
      }
      m_ImGui_TextWrapped(m_ctx, msg.content.c_str());

      // Show Apply button for AI messages with code
      if (!msg.is_user && msg.has_code_block) {
        if (m_ImGui_Button(m_ctx, "Apply Code", nullptr, nullptr)) {
          ApplyCodeBlock(msg.code_block);
        }
      }
      m_ImGui_Separator(m_ctx);
    }

    if (m_waitingForAI) {
      m_ImGui_TextColored(m_ctx, g_theme.dimText, "Thinking...");
    }
  }
  m_ImGui_EndChild(m_ctx);

  // Chat input
  m_ImGui_Separator(m_ctx);

  double inputW = -60; // Leave room for Send button
  int inputFlags = 0;
  m_ImGui_InputText(m_ctx, "##chat_input", m_chatInput, sizeof(m_chatInput),
                    &inputFlags, nullptr);

  double zero = 0;
  double spacing = 5;
  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Send", nullptr, nullptr)) {
    if (strlen(m_chatInput) > 0 && !m_waitingForAI) {
      SendToAI(m_chatInput);
      m_chatInput[0] = '\0';
    }
  }
}

void MagdaJSFXEditor::RenderToolbar() {
  if (m_ImGui_Button(m_ctx, "New", nullptr, nullptr)) {
    NewFile();
  }

  double zero = 0;
  double spacing = 5;
  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Save", nullptr, nullptr)) {
    SaveCurrentFile();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Refresh", nullptr, nullptr)) {
    RefreshFileList();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Test on Track", nullptr, nullptr)) {
    // Add current JSFX to selected track
    if (!m_currentFilePath.empty() && m_rec) {
      // Save first if modified
      if (m_modified) {
        SaveCurrentFile();
      }

      // Get selected track and add FX
      MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
          (MediaTrack * (*)(ReaProject *, int)) m_rec->GetFunc("GetSelectedTrack");
      int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
          (int (*)(MediaTrack *, const char *, bool, int))m_rec->GetFunc(
              "TrackFX_AddByName");

      if (GetSelectedTrack && TrackFX_AddByName) {
        MediaTrack *track = GetSelectedTrack(nullptr, 0);
        if (track) {
          TrackFX_AddByName(track, m_currentFilePath.c_str(), false, -1);

          void (*ShowConsoleMsg)(const char *) =
              (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA JSFX: Added effect to selected track\n");
          }
        }
      }
    }
  }

  // Status on the right
  m_ImGui_SameLine(m_ctx, &zero, &spacing);
  m_ImGui_Dummy(m_ctx, 20, 0);
  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (!m_currentFilePath.empty()) {
    m_ImGui_TextColored(m_ctx, g_theme.dimText, m_currentFilePath.c_str());
  }
}

void MagdaJSFXEditor::SendToAI(const std::string &message) {
  // Add user message to history
  JSFXChatMessage userMsg;
  userMsg.is_user = true;
  userMsg.content = message;
  userMsg.has_code_block = false;
  m_chatHistory.push_back(userMsg);

  m_waitingForAI = true;

  // TODO: Implement actual API call
  // For now, add a placeholder response
  JSFXChatMessage aiMsg;
  aiMsg.is_user = false;
  aiMsg.content = "AI integration coming soon! This will send your code and "
                  "message to the MAGDA API for assistance.";
  aiMsg.has_code_block = false;
  m_chatHistory.push_back(aiMsg);

  m_waitingForAI = false;
}

void MagdaJSFXEditor::ApplyCodeBlock(const std::string &code) {
  // For now, append code at cursor position (simplified)
  // TODO: Smarter insertion based on context
  std::string current = m_editorBuffer;
  current += "\n" + code;
  strncpy(m_editorBuffer, current.c_str(), sizeof(m_editorBuffer) - 1);
  m_modified = true;
}

void MagdaJSFXEditor::ProcessAIResponse(const std::string &response) {
  // Parse response and extract code blocks
  // TODO: Implement proper parsing
}

