#include "magda_jsfx_editor.h"
#include "magda_api_client.h"
#include "magda_login_window.h"
#include "magda_settings_window.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#define REAPER_PLUGIN_DLL_EXPORT
#include "reaper_plugin.h"

// HTTP client for JSFX API calls
static MagdaHTTPClient s_jsfxHttpClient;

MagdaJSFXEditor *g_jsfxEditor = nullptr;

// Theme colors (ABGR format for ImGui)
#define THEME_RGBA(r, g, b) (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))
struct ThemeColors {
  // Text
  int headerText = THEME_RGBA(0xF0, 0xF0, 0xF0);   // Bright white headers
  int normalText = THEME_RGBA(0xD0, 0xD0, 0xD0);   // Light grey text
  int dimText = THEME_RGBA(0x80, 0x80, 0x80);      // Dimmed grey

  // Backgrounds
  int windowBg = THEME_RGBA(0x12, 0x12, 0x16);     // Very dark blue-black
  int childBg = THEME_RGBA(0x1A, 0x1A, 0x22);      // Slightly lighter
  int inputBg = THEME_RGBA(0x22, 0x22, 0x2A);      // Input field background
  int frameBg = THEME_RGBA(0x1E, 0x1E, 0x28);      // Frame background
  int popupBg = THEME_RGBA(0x18, 0x18, 0x20);      // Popup/menu background

  // Electric accent colors (cyan/teal)
  int accent = THEME_RGBA(0x00, 0xD4, 0xE0);       // Electric cyan
  int accentHover = THEME_RGBA(0x20, 0xF0, 0xFF);  // Brighter cyan
  int accentActive = THEME_RGBA(0x00, 0xA0, 0xB0); // Darker cyan when pressed

  // Buttons
  int buttonBg = THEME_RGBA(0x2A, 0x4A, 0x5A);     // Teal-ish button
  int buttonHover = THEME_RGBA(0x35, 0x60, 0x75);  // Lighter on hover
  int buttonActive = THEME_RGBA(0x20, 0x35, 0x45); // Darker on press

  // User/AI chat colors
  int userText = THEME_RGBA(0x80, 0xD0, 0xFF);     // Light blue for user
  int aiText = THEME_RGBA(0x00, 0xE0, 0xA0);       // Electric green for AI

  // Scrollbar
  int scrollbar = THEME_RGBA(0x30, 0x30, 0x40);
  int scrollbarHover = THEME_RGBA(0x50, 0x50, 0x70);
  int scrollbarActive = THEME_RGBA(0x60, 0x60, 0x90);

  // Borders
  int border = THEME_RGBA(0x40, 0x40, 0x55);
  int separator = THEME_RGBA(0x35, 0x35, 0x45);
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
constexpr int TextDisabled = 1;
constexpr int WindowBg = 2;
constexpr int ChildBg = 3;
constexpr int PopupBg = 4;
constexpr int Border = 5;
constexpr int BorderShadow = 6;
constexpr int FrameBg = 7;
constexpr int FrameBgHovered = 8;
constexpr int FrameBgActive = 9;
constexpr int TitleBg = 10;
constexpr int TitleBgActive = 11;
constexpr int TitleBgCollapsed = 12;
constexpr int MenuBarBg = 13;
constexpr int ScrollbarBg = 14;
constexpr int ScrollbarGrab = 15;
constexpr int ScrollbarGrabHovered = 16;
constexpr int ScrollbarGrabActive = 17;
constexpr int CheckMark = 18;
constexpr int SliderGrab = 19;
constexpr int SliderGrabActive = 20;
constexpr int Button = 21;
constexpr int ButtonHovered = 22;
constexpr int ButtonActive = 23;
constexpr int Header = 24;
constexpr int HeaderHovered = 25;
constexpr int HeaderActive = 26;
constexpr int Separator = 27;
constexpr int SeparatorHovered = 28;
constexpr int SeparatorActive = 29;
constexpr int ResizeGrip = 30;
constexpr int ResizeGripHovered = 31;
constexpr int ResizeGripActive = 32;
constexpr int Tab = 33;
constexpr int TabHovered = 34;
constexpr int TabActive = 35;
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

  // Popup/context menu functions
  m_ImGui_BeginPopupContextItem =
      (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopupContextItem");
  m_ImGui_BeginPopupContextWindow =
      (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopupContextWindow");
  m_ImGui_BeginPopup =
      (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopup");
  m_ImGui_OpenPopup =
      (void (*)(void *, const char *, int *))rec->GetFunc("ImGui_OpenPopup");
  m_ImGui_EndPopup = (void (*)(void *))rec->GetFunc("ImGui_EndPopup");
  m_ImGui_MenuItem =
      (bool (*)(void *, const char *, const char *, bool *, bool *))rec->GetFunc("ImGui_MenuItem");
  m_ImGui_CloseCurrentPopup =
      (void (*)(void *))rec->GetFunc("ImGui_CloseCurrentPopup");

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

  // Add parent directory entry ".." for navigation
  std::string effectsFolder = GetEffectsFolder();
  if (m_currentFolder != effectsFolder) {
    JSFXFileEntry parent;
    parent.name = "..";
    size_t lastSlash = m_currentFolder.find_last_of("/");
    if (lastSlash != std::string::npos) {
      parent.full_path = m_currentFolder.substr(0, lastSlash);
    } else {
      parent.full_path = effectsFolder;
    }
    parent.is_directory = true;
    parent.depth = 0;
    parent.is_expanded = false;
    m_files.push_back(parent);
  }

  DIR *dir = opendir(m_currentFolder.c_str());
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.')
      continue; // Skip hidden files and . / ..

    JSFXFileEntry file;
    file.name = entry->d_name;
    file.full_path = m_currentFolder + "/" + entry->d_name;
    file.depth = 0;
    file.is_expanded = false;

    // Use stat for reliable file type detection
    struct stat st;
    if (stat(file.full_path.c_str(), &st) == 0) {
      file.is_directory = S_ISDIR(st.st_mode);
    } else {
      file.is_directory = false;
    }

    // Show all directories and files (JSFX files often have no extension)
    m_files.push_back(file);
  }
  closedir(dir);

  // Sort: directories first (except ..), then alphabetically (case-insensitive)
  std::sort(m_files.begin(), m_files.end(),
            [](const JSFXFileEntry &a, const JSFXFileEntry &b) {
              // Keep ".." at the top
              if (a.name == "..") return true;
              if (b.name == "..") return false;
              // Directories before files
              if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
              }
              // Case-insensitive alphabetical sort
              std::string aLower = a.name;
              std::string bLower = b.name;
              std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
              std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
              return aLower < bLower;
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

void MagdaJSFXEditor::RefreshFXBrowser() {
  if (!m_rec) return;

  // Action 41997 = "Refresh list of JSFX"
  void (*Main_OnCommand)(int, int) =
      (void (*)(int, int))m_rec->GetFunc("Main_OnCommand");
  if (Main_OnCommand) {
    Main_OnCommand(41997, 0);
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
  m_visible = true;
}

void MagdaJSFXEditor::Hide() { m_visible = false; }

void MagdaJSFXEditor::Render() {
  if (!m_available || !m_visible)
    return;

  // Create context if needed
  if (!m_ctx) {
    int flags = 0;
    m_ctx = m_ImGui_CreateContext("JSFX", &flags);
  }

  if (!m_ctx)
    return;

  // Apply electric theme
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::WindowBg, g_theme.windowBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, g_theme.childBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::PopupBg, g_theme.popupBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Border, g_theme.border);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBg, g_theme.frameBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBgHovered, g_theme.inputBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::FrameBgActive, g_theme.inputBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBg, g_theme.windowBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TitleBgActive, g_theme.childBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::MenuBarBg, g_theme.childBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ScrollbarBg, g_theme.scrollbar);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ScrollbarGrab, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ScrollbarGrabHovered, g_theme.accentHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ScrollbarGrabActive, g_theme.accentActive);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::CheckMark, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::SliderGrab, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::SliderGrabActive, g_theme.accentHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, g_theme.buttonBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonHovered, g_theme.buttonHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ButtonActive, g_theme.buttonActive);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Header, g_theme.buttonBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::HeaderHovered, g_theme.buttonHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::HeaderActive, g_theme.buttonActive);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Separator, g_theme.separator);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::SeparatorHovered, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::SeparatorActive, g_theme.accentHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ResizeGrip, g_theme.buttonBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ResizeGripHovered, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ResizeGripActive, g_theme.accentHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Tab, g_theme.buttonBg);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TabHovered, g_theme.buttonHover);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TabActive, g_theme.accent);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Text, g_theme.normalText);
  m_ImGui_PushStyleColor(m_ctx, ImGuiCol::TextDisabled, g_theme.dimText);

  // Set window size
  int condOnce = 2; // ImGuiCond_Once
  m_ImGui_SetNextWindowSize(m_ctx, 1200, 700, &condOnce);

  bool open = true;
  int windowFlags = ImGuiWindowFlags::NoCollapse;

  if (m_ImGui_Begin(m_ctx, "MAGDA JSFX Editor", &open, &windowFlags)) {
    // Toolbar
    RenderToolbar();
    m_ImGui_Separator(m_ctx);

    // Two columns: file panel and editor
    double zero = 0;
    double spacing = 10;

    // File panel on left (fixed width)
    double filePanelW = 200;
    double childH = 0; // auto height
    int childFlags = 0;
    int windowFlags2 = 0;

    if (m_ImGui_BeginChild(m_ctx, "##files", &filePanelW, &childH, &childFlags, &windowFlags2)) {
      RenderFilePanel();
    }
    m_ImGui_EndChild(m_ctx);

    m_ImGui_SameLine(m_ctx, &zero, &spacing);

    // Editor panel (middle, stretch)
    double editorW = -310; // leave room for chat panel
    if (m_ImGui_BeginChild(m_ctx, "##editor", &editorW, &childH, &childFlags, &windowFlags2)) {
      RenderEditorPanel();
      RenderEditorContextMenu();  // Context menu for editor panel
    }
    m_ImGui_EndChild(m_ctx);

    m_ImGui_SameLine(m_ctx, &zero, &spacing);

    // Chat panel on right (fixed width)
    double chatPanelW = 300;
    if (m_ImGui_BeginChild(m_ctx, "##chat", &chatPanelW, &childH, &childFlags, &windowFlags2)) {
      RenderChatPanel();
    }
    m_ImGui_EndChild(m_ctx);
  }
  m_ImGui_End(m_ctx);

  // Render Save As dialog if open
  if (m_showSaveAsDialog) {
    RenderSaveAsDialog();
  }

  // Pop all 35 style colors we pushed
  int styleCount = 35;
  m_ImGui_PopStyleColor(m_ctx, &styleCount);

  // Handle window close
  if (!open) {
    m_visible = false;
    m_ctx = nullptr;
  }
}

void MagdaJSFXEditor::RenderFilePanel() {
  // Show current folder name as header
  std::string folderName;
  size_t lastSlash = m_currentFolder.find_last_of("/");
  if (lastSlash != std::string::npos) {
    folderName = m_currentFolder.substr(lastSlash + 1);
  } else {
    folderName = m_currentFolder;
  }
  m_ImGui_TextColored(m_ctx, g_theme.headerText, folderName.c_str());
  m_ImGui_Separator(m_ctx);

  // File list in a scrollable child
  double childW = 0;  // Fill available width
  double childH = 0;  // Fill available height (0 = auto)
  int childFlags = 0;
  int windowFlags = 0;

  // Track if we need to refresh after the loop (can't modify m_files while iterating)
  std::string pendingNavigate;
  std::string pendingOpenFile;
  bool pendingNewFile = false;
  bool pendingNewFolder = false;
  std::string pendingDelete;

  bool childVisible = m_ImGui_BeginChild(m_ctx, "##file_list", &childW, &childH, &childFlags,
                         &windowFlags);

  // Always render the content - BeginChild might return false but we should still show items
  for (const auto &file : m_files) {
    std::string icon;
    if (file.name == "..") {
      icon = "⬆️ ";
    } else if (file.is_directory) {
      icon = "📁 ";
    } else {
      icon = "📄 ";
    }
    std::string label = icon + file.name;
    std::string itemId = "##file_" + file.full_path;

    bool selected = (file.full_path == m_currentFilePath);
    if (m_ImGui_Selectable(m_ctx, label.c_str(), &selected, nullptr, nullptr,
                           nullptr)) {
      if (file.is_directory) {
        // Queue navigation - don't modify m_files while iterating
        pendingNavigate = file.full_path;
      } else {
        // Queue file open
        pendingOpenFile = file.full_path;
      }
    }

    // Right-click context menu for each file/folder
    if (m_ImGui_BeginPopupContextItem && m_ImGui_MenuItem && m_ImGui_EndPopup) {
      int popupFlags = 1;  // ImGuiPopupFlags_MouseButtonRight
      if (m_ImGui_BeginPopupContextItem(m_ctx, itemId.c_str(), &popupFlags)) {
        if (file.name != "..") {
          if (file.is_directory) {
            // Directory context menu
            if (m_ImGui_MenuItem(m_ctx, "Open", nullptr, nullptr, nullptr)) {
              pendingNavigate = file.full_path;
            }
            if (m_ImGui_MenuItem(m_ctx, "New File Here...", nullptr, nullptr, nullptr)) {
              m_currentFolder = file.full_path;
              pendingNewFile = true;
            }
            if (m_ImGui_MenuItem(m_ctx, "New Folder...", nullptr, nullptr, nullptr)) {
              m_currentFolder = file.full_path;
              pendingNewFolder = true;
            }
            m_ImGui_Separator(m_ctx);
            if (m_ImGui_MenuItem(m_ctx, "Delete Folder", nullptr, nullptr, nullptr)) {
              pendingDelete = file.full_path;
            }
          } else {
            // File context menu
            if (m_ImGui_MenuItem(m_ctx, "Open", nullptr, nullptr, nullptr)) {
              pendingOpenFile = file.full_path;
            }
            if (m_ImGui_MenuItem(m_ctx, "Delete", nullptr, nullptr, nullptr)) {
              pendingDelete = file.full_path;
            }
          }
        }
        m_ImGui_EndPopup(m_ctx);
      }
    }
  }

  // Context menu for file panel background (right-click anywhere in panel)
  if (m_ImGui_BeginPopupContextWindow && m_ImGui_MenuItem && m_ImGui_EndPopup) {
    int popupFlags = 1;  // ImGuiPopupFlags_MouseButtonRight
    if (m_ImGui_BeginPopupContextWindow(m_ctx, "##file_panel_context", &popupFlags)) {
      if (m_ImGui_MenuItem(m_ctx, "New File...", nullptr, nullptr, nullptr)) {
        pendingNewFile = true;
      }
      if (m_ImGui_MenuItem(m_ctx, "New Folder...", nullptr, nullptr, nullptr)) {
        pendingNewFolder = true;
      }
      m_ImGui_Separator(m_ctx);
      if (m_ImGui_MenuItem(m_ctx, "Refresh", nullptr, nullptr, nullptr)) {
        RefreshFileList();
      }
      m_ImGui_EndPopup(m_ctx);
    }
  }

  m_ImGui_EndChild(m_ctx);

  // Process pending actions after iteration is complete
  if (!pendingNavigate.empty()) {
    m_currentFolder = pendingNavigate;
    RefreshFileList();
  }
  if (!pendingOpenFile.empty()) {
    OpenFile(pendingOpenFile);
  }
  if (pendingNewFile) {
    NewFile();
    m_showSaveAsDialog = true;
    strncpy(m_saveAsFilename, "new_effect.jsfx", sizeof(m_saveAsFilename));
  }
  if (pendingNewFolder) {
    m_showSaveAsDialog = true;  // Reuse dialog for folder name
    strncpy(m_saveAsFilename, "New Folder", sizeof(m_saveAsFilename));
    m_contextMenuTarget = "new_folder";
  }
  if (!pendingDelete.empty()) {
    // Delete file or empty folder
    struct stat st;
    if (stat(pendingDelete.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        rmdir(pendingDelete.c_str());  // Only works on empty dirs
      } else {
        remove(pendingDelete.c_str());
      }
      RefreshFileList();
    }
  }
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
  int inputFlags = 0; // No special flags - ReaImGui uses different values

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

void MagdaJSFXEditor::RenderEditorContextMenu() {
  // Right-click context menu for editor (works anywhere in editor panel)
  if (m_ImGui_BeginPopupContextWindow && m_ImGui_MenuItem && m_ImGui_EndPopup) {
    int popupFlags = 1;  // ImGuiPopupFlags_MouseButtonRight
    if (m_ImGui_BeginPopupContextWindow(m_ctx, "##editor_context", &popupFlags)) {
      if (m_ImGui_MenuItem(m_ctx, "Save", "Ctrl+S", nullptr, nullptr)) {
        if (m_currentFilePath.empty()) {
          m_showSaveAsDialog = true;
          strncpy(m_saveAsFilename, m_currentFileName.c_str(), sizeof(m_saveAsFilename));
        } else {
          SaveCurrentFile();
        }
      }
      if (m_ImGui_MenuItem(m_ctx, "Save As...", nullptr, nullptr, nullptr)) {
        m_showSaveAsDialog = true;
        strncpy(m_saveAsFilename, m_currentFileName.c_str(), sizeof(m_saveAsFilename));
      }
      m_ImGui_Separator(m_ctx);
      if (m_ImGui_MenuItem(m_ctx, "Recompile", "F5", nullptr, nullptr)) {
        RecompileJSFX();
      }
      if (m_ImGui_MenuItem(m_ctx, "Add to Selected Track", nullptr, nullptr, nullptr)) {
        AddToSelectedTrack();
      }
      m_ImGui_EndPopup(m_ctx);
    }
  }
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

    int msgIndex = 0;
    for (const auto &msg : m_chatHistory) {
      if (msg.is_user) {
        m_ImGui_TextColored(m_ctx, g_theme.userText, "You:");
      } else {
        m_ImGui_TextColored(m_ctx, g_theme.aiText, "AI:");
      }
      m_ImGui_TextWrapped(m_ctx, msg.content.c_str());

      // Show Apply button for AI messages with code (unique ID per message)
      if (!msg.is_user && msg.has_code_block) {
        // Use index to create unique button ID
        char buttonLabel[64];
        snprintf(buttonLabel, sizeof(buttonLabel), "Apply to Editor##msg%d", msgIndex);
        if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
          ApplyCodeBlock(msg.code_block);
        }
      }
      m_ImGui_Separator(m_ctx);
      msgIndex++;
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
  double zero = 0;
  double spacing = 5;

  // Save button
  if (m_ImGui_Button(m_ctx, "Save", nullptr, nullptr)) {
    if (m_currentFilePath.empty()) {
      m_showSaveAsDialog = true;
      strncpy(m_saveAsFilename, m_currentFileName.c_str(), sizeof(m_saveAsFilename));
    } else {
      SaveCurrentFile();
    }
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  // Add to Track button
  if (m_ImGui_Button(m_ctx, "Add to Track", nullptr, nullptr)) {
    AddToTrackAndOpen();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  // Open in external editor button
  if (m_ImGui_Button(m_ctx, "Open External", nullptr, nullptr)) {
    OpenInReaperEditor();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  // Recompile button
  if (m_ImGui_Button(m_ctx, "Recompile", nullptr, nullptr)) {
    RecompileJSFX();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);
  m_ImGui_Dummy(m_ctx, 20, 0);
  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  // Show current file path/status
  if (!m_currentFilePath.empty()) {
    if (m_modified) {
      std::string modified = m_currentFileName + " *";
      m_ImGui_TextColored(m_ctx, 0xFF88CCFF, modified.c_str());
    } else {
      m_ImGui_TextColored(m_ctx, g_theme.normalText, m_currentFileName.c_str());
    }
  } else if (!m_currentFileName.empty()) {
    std::string unsaved = m_currentFileName + " (unsaved)";
    m_ImGui_TextColored(m_ctx, 0xFF8888FF, unsaved.c_str());
  }
}

void MagdaJSFXEditor::RecompileJSFX() {
  if (!m_rec) return;

  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  if (m_currentFilePath.empty()) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: No file to recompile - please save first\n");
    }
    return;
  }

  // Save the file (this is the key to "recompiling" - REAPER watches the file)
  SaveCurrentFile();

  // JSFX effects automatically recompile when their source file changes
  // The save above triggers this. We just need to confirm to the user.
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Saved %s - any loaded instances will recompile automatically\n",
             m_currentFileName.c_str());
    ShowConsoleMsg(msg);
  }
}

void MagdaJSFXEditor::AddToSelectedTrack() {
  if (!m_rec) return;

  // Save first if modified
  if (m_modified) {
    if (m_currentFilePath.empty()) {
      m_showSaveAsDialog = true;
      strncpy(m_saveAsFilename, m_currentFileName.c_str(), sizeof(m_saveAsFilename));
      return;
    }
    SaveCurrentFile();
  }

  if (m_currentFilePath.empty()) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Please save the file first\n");
    }
    return;
  }

  // Get selected track and add FX
  MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int))m_rec->GetFunc("GetSelectedTrack");
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
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA JSFX: Added %s to selected track\n",
                 m_currentFileName.c_str());
        ShowConsoleMsg(msg);
      }
    } else {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA JSFX: No track selected\n");
      }
    }
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

  // Set backend URL from settings
  const char *backendUrl = MagdaSettingsWindow::GetBackendURL();
  if (backendUrl && backendUrl[0]) {
    s_jsfxHttpClient.SetBackendURL(backendUrl);
  }

  // Set JWT token if available
  const char *token = MagdaLoginWindow::GetStoredToken();
  if (token && token[0]) {
    s_jsfxHttpClient.SetJWTToken(token);
  }

  // Build request JSON
  WDL_FastString requestJson;
  requestJson.Append("{\"message\":\"");

  // Escape message
  for (const char *p = message.c_str(); *p; p++) {
    switch (*p) {
    case '"': requestJson.Append("\\\""); break;
    case '\\': requestJson.Append("\\\\"); break;
    case '\n': requestJson.Append("\\n"); break;
    case '\r': requestJson.Append("\\r"); break;
    case '\t': requestJson.Append("\\t"); break;
    default: requestJson.Append(p, 1); break;
    }
  }
  requestJson.Append("\",\"code\":\"");

  // Escape current code
  for (const char *p = m_editorBuffer; *p; p++) {
    switch (*p) {
    case '"': requestJson.Append("\\\""); break;
    case '\\': requestJson.Append("\\\\"); break;
    case '\n': requestJson.Append("\\n"); break;
    case '\r': requestJson.Append("\\r"); break;
    case '\t': requestJson.Append("\\t"); break;
    default: requestJson.Append(p, 1); break;
    }
  }
  requestJson.Append("\",\"filename\":\"");
  requestJson.Append(m_currentFileName.c_str());
  requestJson.Append("\"}");

  // Make API call in background thread
  std::string requestStr = requestJson.Get();
  std::thread([this, requestStr]() {
    WDL_FastString response, errorMsg;
    bool success = s_jsfxHttpClient.SendPOSTRequest(
        "/api/v1/jsfx/generate", requestStr.c_str(), response, errorMsg, 60);

    // Process response on main thread data structures
    JSFXChatMessage aiMsg;
    aiMsg.is_user = false;

    if (success) {
      // Parse response JSON
      std::string responseStr = response.Get();

      // Check for compile_error (EEL2 validation error)
      size_t compileErrorStart = responseStr.find("\"compile_error\":\"");
      bool hasCompileError = false;
      std::string compileError;

      if (compileErrorStart != std::string::npos) {
        compileErrorStart += 17; // length of "\"compile_error\":\""
        size_t compileErrorEnd = compileErrorStart;
        while (compileErrorEnd < responseStr.length()) {
          if (responseStr[compileErrorEnd] == '"' && responseStr[compileErrorEnd - 1] != '\\') {
            break;
          }
          compileErrorEnd++;
        }
        compileError = responseStr.substr(compileErrorStart, compileErrorEnd - compileErrorStart);
        hasCompileError = !compileError.empty();
      }

      // Extract jsfx_code field
      size_t codeStart = responseStr.find("\"jsfx_code\":\"");
      if (codeStart != std::string::npos) {
        codeStart += 13; // length of "\"jsfx_code\":\""
        size_t codeEnd = codeStart;

        // Find end of string value (handle escapes)
        while (codeEnd < responseStr.length()) {
          if (responseStr[codeEnd] == '"' && responseStr[codeEnd - 1] != '\\') {
            break;
          }
          codeEnd++;
        }

        std::string code = responseStr.substr(codeStart, codeEnd - codeStart);

        // Unescape the code (handles \n, \t, \", \\, and \uXXXX Unicode escapes)
        std::string unescaped;
        for (size_t i = 0; i < code.length(); i++) {
          if (code[i] == '\\' && i + 1 < code.length()) {
            char next = code[i + 1];
            if (next == 'n') { unescaped += '\n'; i++; }
            else if (next == 't') { unescaped += '\t'; i++; }
            else if (next == 'r') { unescaped += '\r'; i++; }
            else if (next == '"') { unescaped += '"'; i++; }
            else if (next == '\\') { unescaped += '\\'; i++; }
            else if (next == 'u' && i + 5 < code.length()) {
              // Unicode escape: \uXXXX
              std::string hex = code.substr(i + 2, 4);
              try {
                int codepoint = std::stoi(hex, nullptr, 16);
                if (codepoint < 128) {
                  // ASCII - single char
                  unescaped += static_cast<char>(codepoint);
                } else if (codepoint < 2048) {
                  // 2-byte UTF-8
                  unescaped += static_cast<char>(0xC0 | (codepoint >> 6));
                  unescaped += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                  // 3-byte UTF-8
                  unescaped += static_cast<char>(0xE0 | (codepoint >> 12));
                  unescaped += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                  unescaped += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                i += 5;  // Skip \uXXXX
              } catch (...) {
                unescaped += code[i];  // Invalid escape, keep as-is
              }
            } else {
              unescaped += code[i];  // Unknown escape, keep backslash
            }
          } else {
            unescaped += code[i];
          }
        }

        if (!unescaped.empty()) {
          if (hasCompileError) {
            // Code generated but has compile errors
            aiMsg.content = "⚠️ Generated JSFX (with compile warning):\n" + compileError;
          } else {
            aiMsg.content = "Generated JSFX code:";
          }
          aiMsg.code_block = unescaped;
          aiMsg.has_code_block = true;
        } else if (hasCompileError) {
          // Compile error without code
          aiMsg.content = "⚠️ EEL2 compile error:\n" + compileError +
                         "\n\nPlease describe what you want differently.";
          aiMsg.has_code_block = false;
        } else {
          aiMsg.content = "JSFX generated but code was empty.";
          aiMsg.has_code_block = false;
        }
      } else if (hasCompileError) {
        // Compile error without code
        aiMsg.content = "⚠️ EEL2 compile error:\n" + compileError +
                       "\n\nPlease try rephrasing your request.";
        aiMsg.has_code_block = false;
      } else {
        aiMsg.content = "Couldn't parse response from server.";
        aiMsg.has_code_block = false;
      }
    } else {
      aiMsg.content = "Error: " + std::string(errorMsg.Get());
      aiMsg.has_code_block = false;
    }

    m_chatHistory.push_back(aiMsg);
    m_waitingForAI = false;
  }).detach();
}

void MagdaJSFXEditor::ApplyCodeBlock(const std::string &code) {
  // Replace entire editor content with the generated JSFX code
  strncpy(m_editorBuffer, code.c_str(), sizeof(m_editorBuffer) - 1);
  m_editorBuffer[sizeof(m_editorBuffer) - 1] = '\0';
  m_modified = true;

  // Auto-save if we have a file path
  if (!m_currentFilePath.empty()) {
    SaveCurrentFile();
    if (m_rec) {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA JSFX: Applied and saved to %s\n", m_currentFileName.c_str());
        ShowConsoleMsg(msg);
      }
    }
  } else {
    // No file path - prompt for save
    m_showSaveAsDialog = true;
    strncpy(m_saveAsFilename, "new_effect.jsfx", sizeof(m_saveAsFilename));
    if (m_rec) {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA JSFX: Applied code - please save the file\n");
      }
    }
  }
}

void MagdaJSFXEditor::AddToTrackAndOpen() {
  if (!m_rec) return;

  // Save first if modified
  if (m_modified) {
    if (m_currentFilePath.empty()) {
      m_showSaveAsDialog = true;
      strncpy(m_saveAsFilename, m_currentFileName.c_str(), sizeof(m_saveAsFilename));
      return;
    }
    SaveCurrentFile();
  }

  if (m_currentFilePath.empty()) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Please save the file first\n");
    }
    return;
  }

  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  // Get selected track
  MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int))m_rec->GetFunc("GetSelectedTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))m_rec->GetFunc("TrackFX_AddByName");
  void (*TrackFX_Show)(MediaTrack *, int, int) =
      (void (*)(MediaTrack *, int, int))m_rec->GetFunc("TrackFX_Show");
  int (*TrackFX_GetCount)(MediaTrack *) =
      (int (*)(MediaTrack *))m_rec->GetFunc("TrackFX_GetCount");

  if (!GetSelectedTrack || !TrackFX_AddByName) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: REAPER API functions not available\n");
    }
    return;
  }

  MediaTrack *track = GetSelectedTrack(nullptr, 0);
  if (!track) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: No track selected - please select a track first\n");
    }
    return;
  }

  // Build the JSFX identifier - REAPER needs path relative to Effects folder with JS: prefix
  std::string effectsFolder = GetEffectsFolder();
  std::string relativePath = m_currentFilePath;

  // Remove the Effects folder prefix to get relative path
  if (m_currentFilePath.find(effectsFolder) == 0) {
    relativePath = m_currentFilePath.substr(effectsFolder.length() + 1);  // +1 for the /
  }

  // Format: "JS:relative/path/to/effect.jsfx"
  std::string fxName = "JS:" + relativePath;

  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Adding FX: %s\n", fxName.c_str());
    ShowConsoleMsg(msg);
  }

  int fxIdx = TrackFX_AddByName(track, fxName.c_str(), false, -1);

  if (fxIdx < 0 && TrackFX_GetCount) {
    // TrackFX_AddByName returns -1 but still adds the FX sometimes
    int count = TrackFX_GetCount(track);
    if (count > 0) {
      fxIdx = count - 1;
    }
  }

  // Open the FX window
  if (fxIdx >= 0 && TrackFX_Show) {
    TrackFX_Show(track, fxIdx, 1);  // 1 = show floating window
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Added %s to track (FX #%d)\n",
               m_currentFileName.c_str(), fxIdx + 1);
      ShowConsoleMsg(msg);
    }
  } else {
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Could not add FX (result: %d)\n", fxIdx);
      ShowConsoleMsg(msg);
    }
  }
}

void MagdaJSFXEditor::OpenInReaperEditor() {
  if (!m_rec || m_currentFilePath.empty()) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Please save the file first\n");
    }
    return;
  }

  // Save if modified
  if (m_modified) {
    SaveCurrentFile();
  }

  // Use REAPER's action to open JSFX in IDE
  // Action 40535 = "Development: Open JSFX in text editor"
  // But we need to use Main_OnCommand with the file
  // Alternative: Use the reascript action
  int (*NamedCommandLookup)(const char *) =
      (int (*)(const char *))m_rec->GetFunc("NamedCommandLookup");
  void (*Main_OnCommand)(int, int) =
      (void (*)(int, int))m_rec->GetFunc("Main_OnCommand");

  // Try to open in system's default editor for JSFX files
  // On macOS we can use "open" command
#ifdef __APPLE__
  std::string cmd = "open \"" + m_currentFilePath + "\"";
  system(cmd.c_str());
#elif _WIN32
  std::string cmd = "start \"\" \"" + m_currentFilePath + "\"";
  system(cmd.c_str());
#else
  std::string cmd = "xdg-open \"" + m_currentFilePath + "\"";
  system(cmd.c_str());
#endif

  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Opened %s in external editor\n",
             m_currentFileName.c_str());
    ShowConsoleMsg(msg);
  }
}

void MagdaJSFXEditor::ProcessAIResponse(const std::string &response) {
  // Parse response and extract code blocks
  // TODO: Implement proper parsing
}

void MagdaJSFXEditor::RenderSaveAsDialog() {
  if (!m_ctx) return;

  // Set dialog size
  int condOnce = 2; // ImGuiCond_Once
  m_ImGui_SetNextWindowSize(m_ctx, 400, 120, &condOnce);

  bool open = true;
  int windowFlags = 0;

  std::string title = (m_contextMenuTarget == "new_folder") ? "New Folder" : "Save As";

  if (m_ImGui_Begin(m_ctx, title.c_str(), &open, &windowFlags)) {
    if (m_contextMenuTarget == "new_folder") {
      m_ImGui_Text(m_ctx, "Folder name:");
    } else {
      m_ImGui_Text(m_ctx, "Filename:");
    }

    int inputFlags = 0;
    m_ImGui_InputText(m_ctx, "##saveas_filename", m_saveAsFilename,
                      sizeof(m_saveAsFilename), &inputFlags, nullptr);

    m_ImGui_Separator(m_ctx);

    double zero = 0;
    double spacing = 10;

    if (m_ImGui_Button(m_ctx, "OK", nullptr, nullptr)) {
      if (strlen(m_saveAsFilename) > 0) {
        if (m_contextMenuTarget == "new_folder") {
          CreateNewFolder(m_saveAsFilename);
        } else {
          // Save As - create new file
          std::string newPath = m_currentFolder + "/" + m_saveAsFilename;
          m_currentFilePath = newPath;
          m_currentFileName = m_saveAsFilename;
          SaveCurrentFile();
          RefreshFileList();    // Update file list to show new file
          RefreshFXBrowser();   // Update REAPER's FX browser
        }
        m_showSaveAsDialog = false;
        m_contextMenuTarget.clear();
      }
    }

    m_ImGui_SameLine(m_ctx, &zero, &spacing);

    if (m_ImGui_Button(m_ctx, "Cancel", nullptr, nullptr)) {
      m_showSaveAsDialog = false;
      m_contextMenuTarget.clear();
    }
  }
  m_ImGui_End(m_ctx);

  if (!open) {
    m_showSaveAsDialog = false;
    m_contextMenuTarget.clear();
  }
}

void MagdaJSFXEditor::CreateNewFolder(const std::string &name) {
  std::string folderPath = m_currentFolder + "/" + name;

#ifdef _WIN32
  mkdir(folderPath.c_str());
#else
  mkdir(folderPath.c_str(), 0755);
#endif

  RefreshFileList();

  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Created folder %s\n", name.c_str());
    ShowConsoleMsg(msg);
  }
}
