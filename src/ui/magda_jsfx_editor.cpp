#include "magda_jsfx_editor.h"
#include "../WDL/WDL/jsonparse.h"
#include "magda_api_client.h"
#include "magda_imgui_login.h"
#include "magda_imgui_settings.h"
#include "magda_openai.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>

#ifdef _WIN32
#include <direct.h>
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

// Theme colors - format is 0xRRGGBBAA (same as main ImGui chat)
#define THEME_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xFF)
struct ThemeColors {
  // Text
  int headerText = THEME_RGBA(0xF0, 0xF0, 0xF0); // Bright white headers
  int normalText = THEME_RGBA(0xD0, 0xD0, 0xD0); // Light grey text
  int dimText = THEME_RGBA(0x80, 0x80, 0x80);    // Dimmed grey

  // Backgrounds (match main chat window style)
  int windowBg = THEME_RGBA(0x3C, 0x3C, 0x3C);   // Dark grey
  int childBg = THEME_RGBA(0x2D, 0x2D, 0x2D);    // Slightly darker panels
  int inputBg = THEME_RGBA(0x1E, 0x1E, 0x1E);    // Dark input
  int frameBg = THEME_RGBA(0x1A, 0x1A, 0x1A);    // Near-black for text areas
  int textAreaBg = THEME_RGBA(0x0A, 0x0A, 0x0A); // Near-black for code editor
  int popupBg = THEME_RGBA(0x2D, 0x2D, 0x2D);    // Popup/menu background

  // Electric accent colors (cyan/teal)
  int accent = THEME_RGBA(0x00, 0xD4, 0xE0);       // Electric cyan
  int accentHover = THEME_RGBA(0x20, 0xF0, 0xFF);  // Brighter cyan
  int accentActive = THEME_RGBA(0x00, 0xA0, 0xB0); // Darker cyan when pressed

  // Buttons
  int buttonBg = THEME_RGBA(0x48, 0x48, 0x48);     // Grey button
  int buttonHover = THEME_RGBA(0x58, 0x58, 0x58);  // Lighter on hover
  int buttonActive = THEME_RGBA(0x38, 0x38, 0x38); // Darker on press

  // User/AI chat colors
  int userText = THEME_RGBA(0x80, 0xD0, 0xFF); // Light blue for user
  int aiText = THEME_RGBA(0x00, 0xE0, 0xA0);   // Electric green for AI

  // Scrollbar
  int scrollbar = THEME_RGBA(0x2D, 0x2D, 0x2D);
  int scrollbarHover = THEME_RGBA(0x48, 0x48, 0x48);
  int scrollbarActive = THEME_RGBA(0x58, 0x58, 0x58);

  // Borders
  int border = THEME_RGBA(0x50, 0x50, 0x50);
  int separator = THEME_RGBA(0x50, 0x50, 0x50);
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
  m_ImGui_CreateContext = (void *(*)(const char *, int *))rec->GetFunc("ImGui_CreateContext");
  m_ImGui_DestroyContext = (void (*)(void *))rec->GetFunc("ImGui_DestroyContext");
  m_ImGui_Begin = (bool (*)(void *, const char *, bool *, int *))rec->GetFunc("ImGui_Begin");
  m_ImGui_End = (void (*)(void *))rec->GetFunc("ImGui_End");
  m_ImGui_Text = (void (*)(void *, const char *))rec->GetFunc("ImGui_Text");
  m_ImGui_TextWrapped = (void (*)(void *, const char *))rec->GetFunc("ImGui_TextWrapped");
  m_ImGui_TextColored = (void (*)(void *, int, const char *))rec->GetFunc("ImGui_TextColored");
  m_ImGui_Button = (bool (*)(void *, const char *, double *, double *))rec->GetFunc("ImGui_Button");
  m_ImGui_Selectable = (bool (*)(void *, const char *, bool *, int *, double *,
                                 double *))rec->GetFunc("ImGui_Selectable");
  m_ImGui_InputText =
      (bool (*)(void *, const char *, char *, int, int *, void *))rec->GetFunc("ImGui_InputText");
  m_ImGui_InputTextMultiline = (bool (*)(void *, const char *, char *, int, double *, double *,
                                         int *, void *))rec->GetFunc("ImGui_InputTextMultiline");
  m_ImGui_Separator = (void (*)(void *))rec->GetFunc("ImGui_Separator");
  m_ImGui_SameLine = (void (*)(void *, double *, double *))rec->GetFunc("ImGui_SameLine");
  m_ImGui_Dummy = (void (*)(void *, double, double))rec->GetFunc("ImGui_Dummy");
  m_ImGui_BeginChild = (bool (*)(void *, const char *, double *, double *, int *,
                                 int *))rec->GetFunc("ImGui_BeginChild");
  m_ImGui_EndChild = (void (*)(void *))rec->GetFunc("ImGui_EndChild");
  m_ImGui_SetNextWindowSize =
      (void (*)(void *, double, double, int *))rec->GetFunc("ImGui_SetNextWindowSize");
  m_ImGui_PushStyleColor = (void (*)(void *, int, int))rec->GetFunc("ImGui_PushStyleColor");
  m_ImGui_PopStyleColor = (void (*)(void *, int *))rec->GetFunc("ImGui_PopStyleColor");
  m_ImGui_GetContentRegionAvail =
      (void (*)(void *, double *, double *))rec->GetFunc("ImGui_GetContentRegionAvail");
  m_ImGui_GetTextLineHeight = (double (*)(void *))rec->GetFunc("ImGui_GetTextLineHeight");
  m_ImGui_BeginGroup = (void (*)(void *))rec->GetFunc("ImGui_BeginGroup");
  m_ImGui_EndGroup = (void (*)(void *))rec->GetFunc("ImGui_EndGroup");
  m_ImGui_BeginTable = (bool (*)(void *, const char *, int, int *, double *, double *,
                                 double *))rec->GetFunc("ImGui_BeginTable");
  m_ImGui_EndTable = (void (*)(void *))rec->GetFunc("ImGui_EndTable");
  m_ImGui_TableNextRow = (void (*)(void *, int *, double *))rec->GetFunc("ImGui_TableNextRow");
  m_ImGui_TableNextColumn = (void (*)(void *))rec->GetFunc("ImGui_TableNextColumn");
  m_ImGui_TableSetupColumn = (void (*)(void *, const char *, int *, double *,
                                       double *))rec->GetFunc("ImGui_TableSetupColumn");
  m_ImGui_GetStyleColor = (int (*)(void *, int))rec->GetFunc("ImGui_GetStyleColor");
  m_ImGui_SetCursorPosY = (void (*)(void *, double))rec->GetFunc("ImGui_SetCursorPosY");
  m_ImGui_GetCursorPosY = (double (*)(void *))rec->GetFunc("ImGui_GetCursorPosY");
  m_ImGui_GetScrollY = (double (*)(void *))rec->GetFunc("ImGui_GetScrollY");
  m_ImGui_SetScrollY = (void (*)(void *, double))rec->GetFunc("ImGui_SetScrollY");
  m_ImGui_GetScrollMaxY = (double (*)(void *))rec->GetFunc("ImGui_GetScrollMaxY");

  // Text wrap control
  m_ImGui_PushTextWrapPos = (void (*)(void *, double *))rec->GetFunc("ImGui_PushTextWrapPos");
  m_ImGui_PopTextWrapPos = (void (*)(void *))rec->GetFunc("ImGui_PopTextWrapPos");

  // Popup/context menu functions
  m_ImGui_BeginPopupContextItem =
      (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopupContextItem");
  m_ImGui_BeginPopupContextWindow =
      (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopupContextWindow");
  m_ImGui_BeginPopup = (bool (*)(void *, const char *, int *))rec->GetFunc("ImGui_BeginPopup");
  m_ImGui_OpenPopup = (void (*)(void *, const char *, int *))rec->GetFunc("ImGui_OpenPopup");
  m_ImGui_EndPopup = (void (*)(void *))rec->GetFunc("ImGui_EndPopup");
  m_ImGui_MenuItem =
      (bool (*)(void *, const char *, const char *, bool *, bool *))rec->GetFunc("ImGui_MenuItem");
  m_ImGui_CloseCurrentPopup = (void (*)(void *))rec->GetFunc("ImGui_CloseCurrentPopup");

  // Keyboard input functions
  m_ImGui_GetKeyMods = (int (*)(void *))rec->GetFunc("ImGui_GetKeyMods");
  m_ImGui_IsKeyPressed = (bool (*)(void *, int, bool *))rec->GetFunc("ImGui_IsKeyPressed");

  // Check if all required functions are available
  m_available = m_ImGui_CreateContext && m_ImGui_Begin && m_ImGui_End && m_ImGui_Text &&
                m_ImGui_Button && m_ImGui_InputTextMultiline && m_ImGui_BeginChild &&
                m_ImGui_EndChild;

  if (m_available) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))rec->GetFunc("ShowConsoleMsg");
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
  std::sort(m_files.begin(), m_files.end(), [](const JSFXFileEntry &a, const JSFXFileEntry &b) {
    // Keep ".." at the top
    if (a.name == "..")
      return true;
    if (b.name == "..")
      return false;
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
  m_currentFileName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
  m_modified = false;

  // Extract description from code
  ExtractDescriptionFromCode();

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Opened %s\n", m_currentFileName.c_str());
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

    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Saved %s\n", m_currentFileName.c_str());
      ShowConsoleMsg(msg);
    }
  }
}

void MagdaJSFXEditor::RefreshFXBrowser() {
  if (!m_rec)
    return;

  // Action 41997 = "Refresh list of JSFX"
  void (*Main_OnCommand)(int, int) = (void (*)(int, int))m_rec->GetFunc("Main_OnCommand");
  if (Main_OnCommand) {
    Main_OnCommand(41997, 0);
  }
}

void MagdaJSFXEditor::NewFile() {
  memset(m_editorBuffer, 0, sizeof(m_editorBuffer));
  memset(m_descriptionBuffer, 0, sizeof(m_descriptionBuffer));
  strncpy(m_descriptionBuffer, "My Effect", sizeof(m_descriptionBuffer) - 1);
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

void MagdaJSFXEditor::ExtractDescriptionFromCode() {
  // Find desc: line and extract description
  memset(m_descriptionBuffer, 0, sizeof(m_descriptionBuffer));

  const char *code = m_editorBuffer;
  const char *descLine = strstr(code, "desc:");
  if (descLine) {
    // Find end of line
    const char *endOfLine = strchr(descLine, '\n');
    if (!endOfLine)
      endOfLine = descLine + strlen(descLine);

    // Extract the description part after "desc:"
    const char *descStart = descLine + 5; // Skip "desc:"
    size_t len = endOfLine - descStart;
    if (len > sizeof(m_descriptionBuffer) - 1)
      len = sizeof(m_descriptionBuffer) - 1;

    strncpy(m_descriptionBuffer, descStart, len);
    m_descriptionBuffer[len] = '\0';

    // Trim leading/trailing whitespace
    char *start = m_descriptionBuffer;
    while (*start == ' ' || *start == '\t')
      start++;
    if (start != m_descriptionBuffer)
      memmove(m_descriptionBuffer, start, strlen(start) + 1);

    char *end = m_descriptionBuffer + strlen(m_descriptionBuffer) - 1;
    while (end > m_descriptionBuffer && (*end == ' ' || *end == '\t' || *end == '\r'))
      *end-- = '\0';
  } else {
    strncpy(m_descriptionBuffer, "Untitled Effect", sizeof(m_descriptionBuffer) - 1);
  }
}

void MagdaJSFXEditor::UpdateDescriptionInCode() {
  // Find and replace the desc: line in the editor buffer
  std::string code(m_editorBuffer);
  size_t descPos = code.find("desc:");
  if (descPos != std::string::npos) {
    // Find end of the desc line
    size_t endOfLine = code.find('\n', descPos);
    if (endOfLine == std::string::npos)
      endOfLine = code.length();

    // Replace the desc line
    std::string newDescLine = "desc:" + std::string(m_descriptionBuffer);
    code.replace(descPos, endOfLine - descPos, newDescLine);

    strncpy(m_editorBuffer, code.c_str(), sizeof(m_editorBuffer) - 1);
    m_editorBuffer[sizeof(m_editorBuffer) - 1] = '\0';
  } else {
    // No desc: line found, add one at the beginning
    std::string newCode = "desc:" + std::string(m_descriptionBuffer) + "\n" + code;
    strncpy(m_editorBuffer, newCode.c_str(), sizeof(m_editorBuffer) - 1);
    m_editorBuffer[sizeof(m_editorBuffer) - 1] = '\0';
  }
}

void MagdaJSFXEditor::Show() {
  m_visible = true;
}

void MagdaJSFXEditor::Hide() {
  m_visible = false;
}

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

  // Load color index functions from ReaImGui (same pattern as main chat)
  int (*Col_WindowBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_WindowBg");
  int (*Col_ChildBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_ChildBg");
  int (*Col_Text)() = (int (*)())m_rec->GetFunc("ImGui_Col_Text");
  int (*Col_FrameBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_FrameBg");
  int (*Col_FrameBgHovered)() = (int (*)())m_rec->GetFunc("ImGui_Col_FrameBgHovered");
  int (*Col_FrameBgActive)() = (int (*)())m_rec->GetFunc("ImGui_Col_FrameBgActive");
  int (*Col_Button)() = (int (*)())m_rec->GetFunc("ImGui_Col_Button");
  int (*Col_ButtonHovered)() = (int (*)())m_rec->GetFunc("ImGui_Col_ButtonHovered");
  int (*Col_ButtonActive)() = (int (*)())m_rec->GetFunc("ImGui_Col_ButtonActive");
  int (*Col_Border)() = (int (*)())m_rec->GetFunc("ImGui_Col_Border");
  int (*Col_Separator)() = (int (*)())m_rec->GetFunc("ImGui_Col_Separator");
  int (*Col_ScrollbarBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_ScrollbarBg");
  int (*Col_ScrollbarGrab)() = (int (*)())m_rec->GetFunc("ImGui_Col_ScrollbarGrab");

  // Apply theme colors
  int styleColorCount = 0;
  if (Col_WindowBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_WindowBg(), g_theme.windowBg);
    styleColorCount++;
  }
  if (Col_ChildBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_ChildBg(), g_theme.childBg);
    styleColorCount++;
  }
  if (Col_Text) {
    m_ImGui_PushStyleColor(m_ctx, Col_Text(), g_theme.normalText);
    styleColorCount++;
  }
  if (Col_FrameBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_FrameBg(), g_theme.inputBg);
    styleColorCount++;
  }
  if (Col_FrameBgHovered) {
    m_ImGui_PushStyleColor(m_ctx, Col_FrameBgHovered(), g_theme.buttonHover);
    styleColorCount++;
  }
  if (Col_FrameBgActive) {
    m_ImGui_PushStyleColor(m_ctx, Col_FrameBgActive(), g_theme.buttonBg);
    styleColorCount++;
  }
  if (Col_Button) {
    m_ImGui_PushStyleColor(m_ctx, Col_Button(), g_theme.buttonBg);
    styleColorCount++;
  }
  if (Col_ButtonHovered) {
    m_ImGui_PushStyleColor(m_ctx, Col_ButtonHovered(), g_theme.buttonHover);
    styleColorCount++;
  }
  if (Col_ButtonActive) {
    m_ImGui_PushStyleColor(m_ctx, Col_ButtonActive(), g_theme.childBg);
    styleColorCount++;
  }
  if (Col_Border) {
    m_ImGui_PushStyleColor(m_ctx, Col_Border(), g_theme.border);
    styleColorCount++;
  }
  if (Col_Separator) {
    m_ImGui_PushStyleColor(m_ctx, Col_Separator(), g_theme.separator);
    styleColorCount++;
  }
  if (Col_ScrollbarBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_ScrollbarBg(), g_theme.childBg);
    styleColorCount++;
  }
  if (Col_ScrollbarGrab) {
    m_ImGui_PushStyleColor(m_ctx, Col_ScrollbarGrab(), g_theme.buttonBg);
    styleColorCount++;
  }

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
    double editorW = -510; // leave room for 500px chat panel + 10px spacing
    if (m_ImGui_BeginChild(m_ctx, "##editor", &editorW, &childH, &childFlags, &windowFlags2)) {
      RenderEditorPanel();
      RenderEditorContextMenu(); // Context menu for editor panel
    }
    m_ImGui_EndChild(m_ctx);

    m_ImGui_SameLine(m_ctx, &zero, &spacing);

    // Chat panel on right (fixed width, wider for AI output + scrollbar)
    double chatPanelW = 500;
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

  // Pop style colors
  if (styleColorCount > 0) {
    m_ImGui_PopStyleColor(m_ctx, &styleColorCount);
  }

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
  double childW = 0; // Fill available width
  double childH = 0; // Fill available height (0 = auto)
  int childFlags = 0;
  int windowFlags = 0;

  // Track if we need to refresh after the loop (can't modify m_files while
  // iterating)
  std::string pendingNavigate;
  std::string pendingOpenFile;
  bool pendingNewFile = false;
  bool pendingNewFolder = false;
  std::string pendingDelete;

  bool childVisible =
      m_ImGui_BeginChild(m_ctx, "##file_list", &childW, &childH, &childFlags, &windowFlags);

  // Always render the content - BeginChild might return false but we should
  // still show items
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
    if (m_ImGui_Selectable(m_ctx, label.c_str(), &selected, nullptr, nullptr, nullptr)) {
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
      int popupFlags = 1; // ImGuiPopupFlags_MouseButtonRight
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
    int popupFlags = 1; // ImGuiPopupFlags_MouseButtonRight
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
    m_showSaveAsDialog = true; // Reuse dialog for folder name
    strncpy(m_saveAsFilename, "New Folder", sizeof(m_saveAsFilename));
    m_contextMenuTarget = "new_folder";
  }
  if (!pendingDelete.empty()) {
    // Delete file or empty folder
    struct stat st;
    if (stat(pendingDelete.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        rmdir(pendingDelete.c_str()); // Only works on empty dirs
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
  int inputFlags = 0;  // No special flags - ReaImGui uses different values

  // Dark background for code - use dynamic color index
  int (*Col_FrameBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_FrameBg");
  if (Col_FrameBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_FrameBg(), g_theme.textAreaBg);
  }

  if (m_ImGui_InputTextMultiline(m_ctx, "##code_editor", m_editorBuffer, sizeof(m_editorBuffer),
                                 &editorW, &editorH, &inputFlags, nullptr)) {
    m_modified = true;
  }

  if (Col_FrameBg) {
    int one = 1;
    m_ImGui_PopStyleColor(m_ctx, &one);
  }
}

void MagdaJSFXEditor::RenderEditorContextMenu() {
  // Right-click context menu for editor (works anywhere in editor panel)
  if (m_ImGui_BeginPopupContextWindow && m_ImGui_MenuItem && m_ImGui_EndPopup) {
    int popupFlags = 1; // ImGuiPopupFlags_MouseButtonRight
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
  double chatW = 0;    // Full width
  double chatH = -60;  // Leave room for input
  int childFlags = 0;  // No border
  int windowFlags = 0; // No forced scrollbar - let it appear when needed

  if (m_ImGui_BeginChild(m_ctx, "##chat_history", &chatW, &chatH, &childFlags, &windowFlags)) {
    // Add padding around content
    m_ImGui_Dummy(m_ctx, 0, 5); // Top padding

    // Inner container with left and right padding
    double innerW = -8; // 8px right padding
    double innerH = 0;
    int innerFlags = 0;
    int innerWinFlags = 0;

    m_ImGui_Dummy(m_ctx, 1, 0); // 1px left padding
    m_ImGui_SameLine(m_ctx, nullptr, nullptr);

    if (m_ImGui_BeginChild(m_ctx, "##chat_content", &innerW, &innerH, &innerFlags,
                           &innerWinFlags)) {
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

        // Show AI response with description and code block
        if (!msg.is_user && msg.has_code_block && !msg.code_block.empty()) {
          // Show description if available (visually distinct with accent color)
          if (!msg.description.empty()) {
            // Description header
            m_ImGui_TextColored(m_ctx, g_theme.accent, "💡 About this effect:");
            m_ImGui_Dummy(m_ctx, 0, 3);

            // Description text with normal color (stands out from dimmed code)
            m_ImGui_TextWrapped(m_ctx, msg.description.c_str());
            m_ImGui_Dummy(m_ctx, 0, 8); // More spacing before code
          } else {
            // No description - show generic message
            m_ImGui_TextWrapped(m_ctx, msg.content.c_str());
            m_ImGui_Dummy(m_ctx, 0, 5);
          }

          // Visual separator between description and code
          m_ImGui_Separator(m_ctx);
          m_ImGui_TextColored(m_ctx, g_theme.dimText, "📄 Generated JSFX code:");
          m_ImGui_Dummy(m_ctx, 0, 3);

          // Display code preview with sliding window (show last 400 chars while
          // streaming)
          std::string preview = msg.code_block;
          if (preview.length() > 400) {
            if (!msg.streaming_complete) {
              // While streaming: show last 400 chars (sliding window)
              preview = "... (" + std::to_string(msg.code_block.length()) + " chars)\n" +
                        preview.substr(preview.length() - 400);
            } else {
              // After streaming: show first 400 chars with total count
              preview = preview.substr(0, 400) + "\n... (" +
                        std::to_string(msg.code_block.length()) + " chars total)";
            }
          }

          // Code preview with dimmer color (clearly different from description)
          m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Text, g_theme.dimText);
          m_ImGui_TextWrapped(m_ctx, preview.c_str());
          m_ImGui_PopStyleColor(m_ctx, nullptr);

          m_ImGui_Dummy(m_ctx, 0, 8);

          // Show compile error if any
          if (!msg.compile_error.empty()) {
            m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Text, 0xFF4444FF); // Red
            m_ImGui_TextWrapped(m_ctx, ("⚠️ Compile Error: " + msg.compile_error).c_str());
            m_ImGui_PopStyleColor(m_ctx, nullptr);
            m_ImGui_Dummy(m_ctx, 0, 4);
          }

          // Buttons - only enabled when streaming is complete
          char buttonLabel[64];
          if (msg.streaming_complete) {
            // Apply to Editor button
            snprintf(buttonLabel, sizeof(buttonLabel), "Apply to Editor##msg%d", msgIndex);
            if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
              ApplyCodeBlock(msg.code_block);
            }

            m_ImGui_SameLine(m_ctx, nullptr, nullptr);

            // Try Compile button
            snprintf(buttonLabel, sizeof(buttonLabel), "Try Compile##msg%d", msgIndex);
            if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
              std::string error = TryCompileJSFX(msg.code_block);
              // Update the message with compile result
              // Note: Using msgIndex-1 because we incremented at loop start
              if (msgIndex > 0 && msgIndex <= m_chatHistory.size()) {
                m_chatHistory[msgIndex - 1].compile_error = error;
                m_chatHistory[msgIndex - 1].compile_checked = true;
              }
            }

            // Fix Errors / Auto-fix buttons - only show if there's a compile
            // error
            if (!msg.compile_error.empty()) {
              m_ImGui_SameLine(m_ctx, nullptr, nullptr);

              // Check if auto-fix is active for this message
              bool isAutoFixTarget = m_autoFixActive && (m_autoFixMessageIndex == msgIndex - 1 ||
                                                         m_autoFixMessageIndex == msgIndex);

              if (isAutoFixTarget) {
                // Show auto-fix progress
                m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button,
                                       0xFF4488FF); // Blue
                char progressLabel[64];
                snprintf(progressLabel, sizeof(progressLabel), "Auto-fixing (%d/%d)##msg%d",
                         m_autoFixAttempt, MAX_AUTO_FIX_ATTEMPTS, msgIndex);
                if (m_ImGui_Button(m_ctx, progressLabel, nullptr, nullptr)) {
                  StopAutoFix();
                }
                m_ImGui_PopStyleColor(m_ctx, nullptr);

                m_ImGui_SameLine(m_ctx, nullptr, nullptr);
                m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF2222AA);
                snprintf(buttonLabel, sizeof(buttonLabel), "Stop##msg%d", msgIndex);
                if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
                  StopAutoFix();
                }
                m_ImGui_PopStyleColor(m_ctx, nullptr);
              } else if (!m_autoFixActive && !m_waitingForAI) {
                // Manual fix button
                m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button,
                                       0xFF2222AA); // Red
                snprintf(buttonLabel, sizeof(buttonLabel), "Fix Errors##msg%d", msgIndex);
                if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
                  RequestFix(msgIndex - 1, msg.compile_error);
                }
                m_ImGui_PopStyleColor(m_ctx, nullptr);

                // Auto-fix button
                m_ImGui_SameLine(m_ctx, nullptr, nullptr);
                m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button,
                                       0xFF44AA44); // Green
                snprintf(buttonLabel, sizeof(buttonLabel), "Auto-fix##msg%d", msgIndex);
                if (m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr)) {
                  StartAutoFix(msgIndex - 1);
                }
                m_ImGui_PopStyleColor(m_ctx, nullptr);
              }
            } else if (msg.compile_checked) {
              // Show success indicator
              m_ImGui_SameLine(m_ctx, nullptr, nullptr);
              if (msg.auto_fix_attempt > 0) {
                char successLabel[64];
                snprintf(successLabel, sizeof(successLabel), "✓ Fixed (attempt %d)",
                         msg.auto_fix_attempt);
                m_ImGui_TextColored(m_ctx, 0xFF44FF44, successLabel);
              } else {
                m_ImGui_TextColored(m_ctx, 0xFF44FF44, "✓ Compiles OK");
              }
            }
          } else {
            // Show disabled button while streaming
            m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, g_theme.buttonBg & 0x80808080);
            m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Text, g_theme.dimText);
            snprintf(buttonLabel, sizeof(buttonLabel), "Streaming...##msg%d", msgIndex);
            m_ImGui_Button(m_ctx, buttonLabel, nullptr, nullptr);
            m_ImGui_PopStyleColor(m_ctx, nullptr);
            m_ImGui_PopStyleColor(m_ctx, nullptr);
          }
        } else {
          // Regular message content (user messages or AI messages without code)
          m_ImGui_TextWrapped(m_ctx, msg.content.c_str());
        }
        m_ImGui_Separator(m_ctx);
        msgIndex++;
      }

      // Show loading spinner while waiting for AI
      if (m_waitingForAI) {
        // Animated spinner using braille dots: ⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏
        const char *spinnerFrames[] = {
            "\xe2\xa0\x8b", // ⠋
            "\xe2\xa0\x99", // ⠙
            "\xe2\xa0\xb9", // ⠹
            "\xe2\xa0\xb8", // ⠸
            "\xe2\xa0\xbc", // ⠼
            "\xe2\xa0\xb4", // ⠴
            "\xe2\xa0\xa6", // ⠦
            "\xe2\xa0\xa7", // ⠧
            "\xe2\xa0\x87", // ⠇
            "\xe2\xa0\x8f"  // ⠏
        };
        const int numFrames = 10;
        double elapsed = ((double)clock() / CLOCKS_PER_SEC) - m_spinnerStartTime;
        int frameIndex = ((int)(elapsed * 10.0)) % numFrames; // 10 FPS animation

        char loadingMsg[128];
        snprintf(loadingMsg, sizeof(loadingMsg), "%s Generating JSFX...",
                 spinnerFrames[frameIndex]);
        m_ImGui_TextColored(m_ctx, g_theme.accent, loadingMsg);
      }
    }
    m_ImGui_EndChild(m_ctx); // End ##chat_content
  }
  m_ImGui_EndChild(m_ctx); // End ##chat_history

  // Chat input section
  m_ImGui_Separator(m_ctx);

  // Small top padding
  m_ImGui_Dummy(m_ctx, 0, 4);

  // Get available width
  double availW = 0, availH = 0;
  m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);

  // Multiline input - full width minus button space
  double inputW = availW - 55;
  double inputH = 38; // Smaller height
  int inputFlags = 0;

  // Dark input background
  int (*Col_FrameBg)() = (int (*)())m_rec->GetFunc("ImGui_Col_FrameBg");
  if (Col_FrameBg) {
    m_ImGui_PushStyleColor(m_ctx, Col_FrameBg(), 0x1E1E1EFF); // Very dark
  }

  m_ImGui_InputTextMultiline(m_ctx, "##chat_input", m_chatInput, sizeof(m_chatInput), &inputW,
                             &inputH, &inputFlags, nullptr);

  if (Col_FrameBg) {
    int n = 1;
    m_ImGui_PopStyleColor(m_ctx, &n);
  }

  // Send button next to input
  double zero = 0;
  double spacing = 6;
  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  // Dark grey button
  int (*Col_Button)() = (int (*)())m_rec->GetFunc("ImGui_Col_Button");
  int (*Col_ButtonHovered)() = (int (*)())m_rec->GetFunc("ImGui_Col_ButtonHovered");
  int (*Col_ButtonActive)() = (int (*)())m_rec->GetFunc("ImGui_Col_ButtonActive");
  int btnStyleCount = 0;
  if (Col_Button && Col_ButtonHovered && Col_ButtonActive) {
    m_ImGui_PushStyleColor(m_ctx, Col_Button(), 0x484848FF); // Dark grey
    m_ImGui_PushStyleColor(m_ctx, Col_ButtonHovered(),
                           0x585858FF); // Lighter on hover
    m_ImGui_PushStyleColor(m_ctx, Col_ButtonActive(),
                           0x383838FF); // Darker on press
    btnStyleCount = 3;
  }

  double buttonW = 42;
  double buttonH = 38;
  if (m_ImGui_Button(m_ctx, ">>", &buttonW, &buttonH)) {
    if (strlen(m_chatInput) > 0 && !m_waitingForAI) {
      SendToAI(m_chatInput);
      m_chatInput[0] = '\0';
    }
  }

  if (btnStyleCount > 0) {
    m_ImGui_PopStyleColor(m_ctx, &btnStyleCount);
  }

  // Bottom padding
  m_ImGui_Dummy(m_ctx, 0, 4);
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
  if (!m_rec)
    return;

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  if (m_currentFilePath.empty()) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: No file to recompile - please save first\n");
    }
    return;
  }

  // Save the file first
  SaveCurrentFile();

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA JSFX: Recompiling...\n");
  }

  // To actually compile and check for errors, we need to add to a track
  // Use AddToTrackAndOpen which handles error detection
  AddToTrackAndOpen();
}

void MagdaJSFXEditor::AddToSelectedTrack() {
  if (!m_rec)
    return;

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
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Please save the file first\n");
    }
    return;
  }

  // Get selected track and add FX
  MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) m_rec->GetFunc("GetSelectedTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))m_rec->GetFunc("TrackFX_AddByName");

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

  // Add placeholder AI message for streaming updates
  JSFXChatMessage aiPlaceholder;
  aiPlaceholder.is_user = false;
  aiPlaceholder.content = "Generating JSFX...";
  aiPlaceholder.has_code_block = false;
  size_t aiIndex = m_chatHistory.size();
  m_chatHistory.push_back(aiPlaceholder);

  m_waitingForAI = true;
  m_spinnerStartTime = (double)clock() / CLOCKS_PER_SEC;

  // Check if we can use direct OpenAI streaming (preferred - faster, no Go API
  // needed)
  MagdaOpenAI *openai = GetMagdaOpenAI();
  bool useDirectOpenAI = openai && openai->HasAPIKey();

  // Capture message and code for the background thread
  std::string userMessage = message;
  std::string existingCode = m_editorBuffer;

  if (useDirectOpenAI) {
    // Direct OpenAI streaming - no Go API needed
    if (m_rec) {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA JSFX: Using direct OpenAI streaming...\n");
      }
    }

    std::thread([this, userMessage, existingCode, aiIndex]() {
      std::string codeBuffer;
      WDL_FastString errorMsg;

      auto streamCallback = [this, aiIndex, &codeBuffer](const char *text, bool is_done) -> bool {
        if (text && *text) {
          codeBuffer += text;
          // Update UI with streaming code
          if (aiIndex < m_chatHistory.size()) {
            JSFXChatMessage &aiMsg = m_chatHistory[aiIndex];
            aiMsg.content = "Streaming JSFX code...";
            aiMsg.code_block = codeBuffer;
            aiMsg.has_code_block = true;
          }
        }
        if (is_done) {
          // Streaming complete
          if (aiIndex < m_chatHistory.size()) {
            JSFXChatMessage &aiMsg = m_chatHistory[aiIndex];
            aiMsg.code_block = codeBuffer;
            aiMsg.has_code_block = !codeBuffer.empty();
            aiMsg.streaming_complete = true;
            if (!codeBuffer.empty()) {
              aiMsg.content = "Generated JSFX code.";
            } else {
              aiMsg.content = "JSFX generation finished with empty result.";
            }
          }
          m_waitingForAI = false;
        }
        return true;
      };

      MagdaOpenAI *openai = GetMagdaOpenAI();
      bool success = openai->GenerateJSFXStream(
          userMessage.c_str(), existingCode.empty() ? nullptr : existingCode.c_str(),
          streamCallback, errorMsg);

      if (!success && aiIndex < m_chatHistory.size()) {
        JSFXChatMessage &aiMsg = m_chatHistory[aiIndex];
        aiMsg.content = "Error: " + std::string(errorMsg.Get());
        aiMsg.has_code_block = false;
        m_waitingForAI = false;
      }
    }).detach();

    return;
  }

  // Fall back to Go API if no OpenAI key
  // Set backend URL from settings
  const char *backendUrl = MagdaImGuiLogin::GetBackendURL();
  if (backendUrl && backendUrl[0]) {
    s_jsfxHttpClient.SetBackendURL(backendUrl);
  }

  // Only set JWT token if auth is required (Gateway mode)
  // Local API (AuthMode::None) doesn't need authentication
  if (g_imguiLogin && g_imguiLogin->GetAuthMode() == AuthMode::Gateway) {
    const char *token = MagdaImGuiLogin::GetStoredToken();
    if (token && token[0]) {
      s_jsfxHttpClient.SetJWTToken(token);
    }
  } else {
    s_jsfxHttpClient.SetJWTToken(nullptr);
  }

  // Build request JSON
  WDL_FastString requestJson;
  requestJson.Append("{\"message\":\"");

  // Escape message
  for (const char *p = message.c_str(); *p; p++) {
    switch (*p) {
    case '"':
      requestJson.Append("\\\"");
      break;
    case '\\':
      requestJson.Append("\\\\");
      break;
    case '\n':
      requestJson.Append("\\n");
      break;
    case '\r':
      requestJson.Append("\\r");
      break;
    case '\t':
      requestJson.Append("\\t");
      break;
    default:
      requestJson.Append(p, 1);
      break;
    }
  }
  requestJson.Append("\",\"code\":\"");

  // Escape current code
  for (const char *p = m_editorBuffer; *p; p++) {
    switch (*p) {
    case '"':
      requestJson.Append("\\\"");
      break;
    case '\\':
      requestJson.Append("\\\\");
      break;
    case '\n':
      requestJson.Append("\\n");
      break;
    case '\r':
      requestJson.Append("\\r");
      break;
    case '\t':
      requestJson.Append("\\t");
      break;
    default:
      requestJson.Append(p, 1);
      break;
    }
  }
  requestJson.Append("\",\"filename\":\"");
  requestJson.Append(m_currentFileName.c_str());

  // Add include_description setting
  bool includeDesc = MagdaImGuiSettings::GetJSFXIncludeDescription();
  requestJson.Append("\",\"include_description\":");
  requestJson.Append(includeDesc ? "true" : "false");
  requestJson.Append("}");

  // Make API call in background thread
  std::string requestStr = requestJson.Get();
  std::thread([this, requestStr, aiIndex]() {
    WDL_FastString errorMsg;

    struct JSFXStreamContext {
      MagdaJSFXEditor *editor;
      size_t messageIndex;
      std::string codeBuffer;
    } ctx{this, aiIndex, ""};

    auto sseCallback = [](const char *event_json, void *user_data) {
      JSFXStreamContext *ctx = static_cast<JSFXStreamContext *>(user_data);
      if (!ctx || !ctx->editor || ctx->messageIndex >= ctx->editor->m_chatHistory.size()) {
        return;
      }

      MagdaJSFXEditor *editor = ctx->editor;
      wdl_json_parser parser;
      wdl_json_element *root = parser.parse(event_json, (int)strlen(event_json));

      if (!parser.m_err && root) {
        const char *eventType = nullptr;
        if (wdl_json_element *type_elem = root->get_item_by_name("type")) {
          if (type_elem->m_value_string) {
            eventType = type_elem->m_value;
          }
        }

        if (eventType && strcmp(eventType, "chunk") == 0) {
          // TRUE STREAMING: receive character chunks as they arrive from LLM
          if (wdl_json_element *chunk_elem = root->get_item_by_name("chunk")) {
            if (chunk_elem->m_value_string) {
              ctx->codeBuffer.append(chunk_elem->m_value);
              JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
              aiMsg.content = "Streaming JSFX code...";
              aiMsg.code_block = ctx->codeBuffer;
              aiMsg.has_code_block = true;
            }
          }
        } else if (eventType && strcmp(eventType, "line") == 0) {
          // LEGACY: line-by-line streaming (fallback compatibility)
          if (wdl_json_element *line_elem = root->get_item_by_name("line")) {
            if (line_elem->m_value_string) {
              ctx->codeBuffer.append(line_elem->m_value);
              ctx->codeBuffer.append("\n");
              JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
              aiMsg.content = "Streaming JSFX code...";
              aiMsg.code_block = ctx->codeBuffer;
              aiMsg.has_code_block = true;
            }
          }
        } else if (eventType && strcmp(eventType, "done") == 0) {
          std::string finalCode = ctx->codeBuffer;
          if (wdl_json_element *code_elem = root->get_item_by_name("jsfx_code")) {
            if (code_elem->m_value_string) {
              finalCode = code_elem->m_value;
            }
          }

          // Extract description
          std::string description;
          if (wdl_json_element *desc_elem = root->get_item_by_name("description")) {
            if (desc_elem->m_value_string && desc_elem->m_value[0]) {
              description = desc_elem->m_value;
            }
          }

          const char *compileErr = "";
          if (wdl_json_element *compile_elem = root->get_item_by_name("compile_error")) {
            if (compile_elem->m_value_string) {
              compileErr = compile_elem->m_value;
            }
          }

          const char *messageTxt = nullptr;
          if (wdl_json_element *msg_elem = root->get_item_by_name("message")) {
            if (msg_elem->m_value_string) {
              messageTxt = msg_elem->m_value;
            }
          }

          JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
          aiMsg.code_block = finalCode;
          aiMsg.description = description;
          aiMsg.has_code_block = !finalCode.empty();
          aiMsg.streaming_complete = true; // Mark streaming as done

          if (compileErr && strlen(compileErr) > 0) {
            aiMsg.content = std::string("⚠️ ") + compileErr;
          } else if (!description.empty()) {
            aiMsg.content = description;
          } else if (messageTxt) {
            aiMsg.content = messageTxt;
          } else if (!finalCode.empty()) {
            aiMsg.content = "Generated JSFX code.";
          } else {
            aiMsg.content = "JSFX generation finished with empty result.";
            aiMsg.has_code_block = false;
          }

          editor->m_waitingForAI = false;
        } else if (eventType && strcmp(eventType, "start") == 0) {
          if (wdl_json_element *msg_elem = root->get_item_by_name("message")) {
            if (msg_elem->m_value_string) {
              editor->m_chatHistory[ctx->messageIndex].content = msg_elem->m_value;
            }
          }
        } else if (eventType && strcmp(eventType, "error") == 0) {
          const char *msg = "Streaming error";
          if (wdl_json_element *msg_elem = root->get_item_by_name("message")) {
            if (msg_elem->m_value_string) {
              msg = msg_elem->m_value;
            }
          }
          JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
          aiMsg.content = std::string("Error: ") + msg;
          aiMsg.has_code_block = false;
          editor->m_waitingForAI = false;
        }
      } else {
        // Fallback: treat raw event as streamed text
        ctx->codeBuffer.append(event_json);
        ctx->codeBuffer.append("\n");
        JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
        aiMsg.content = "Streaming JSFX code...";
        aiMsg.code_block = ctx->codeBuffer;
        aiMsg.has_code_block = true;
      }
    };

    bool success = s_jsfxHttpClient.SendPOSTStream("/api/v1/jsfx/generate/stream",
                                                   requestStr.c_str(), sseCallback, &ctx, errorMsg,
                                                   180); // 3 minutes - CFG grammar can be slow

    if (!success) {
      JSFXChatMessage &aiMsg = m_chatHistory[aiIndex];
      aiMsg.content = "Error: " + std::string(errorMsg.Get());
      aiMsg.has_code_block = false;
    }

    m_waitingForAI = false;
  }).detach();
}

void MagdaJSFXEditor::ApplyCodeBlock(const std::string &code) {
  // Replace entire editor content with the generated JSFX code
  strncpy(m_editorBuffer, code.c_str(), sizeof(m_editorBuffer) - 1);
  m_editorBuffer[sizeof(m_editorBuffer) - 1] = '\0';
  m_modified = true;

  // Extract description from the generated code
  ExtractDescriptionFromCode();

  // Auto-save if we have a file path
  if (!m_currentFilePath.empty()) {
    SaveCurrentFile();
    if (m_rec) {
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA JSFX: Applied and saved to %s\n",
                 m_currentFileName.c_str());
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
  if (!m_rec)
    return;

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
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Please save the file first\n");
    }
    return;
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  // Get selected track
  MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) m_rec->GetFunc("GetSelectedTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))m_rec->GetFunc("TrackFX_AddByName");
  void (*TrackFX_Show)(MediaTrack *, int, int) =
      (void (*)(MediaTrack *, int, int))m_rec->GetFunc("TrackFX_Show");
  int (*TrackFX_GetCount)(MediaTrack *) = (int (*)(MediaTrack *))m_rec->GetFunc("TrackFX_GetCount");

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

  // Build the JSFX identifier - REAPER needs path relative to Effects folder
  // with JS: prefix
  std::string effectsFolder = GetEffectsFolder();
  std::string relativePath = m_currentFilePath;

  // Remove the Effects folder prefix to get relative path
  if (m_currentFilePath.find(effectsFolder) == 0) {
    relativePath = m_currentFilePath.substr(effectsFolder.length() + 1); // +1 for the /
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
    TrackFX_Show(track, fxIdx, 1); // 1 = show floating window
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Added %s to track (FX #%d)\n",
               m_currentFileName.c_str(), fxIdx + 1);
      ShowConsoleMsg(msg);
    }

    // Check for compile error using TrackFX_GetNamedConfigParm
    std::string compileError;

    bool (*TrackFX_GetNamedConfigParm)(MediaTrack *, int, const char *, char *, int) = (bool (*)(
        MediaTrack *, int, const char *, char *, int))m_rec->GetFunc("TrackFX_GetNamedConfigParm");

    if (TrackFX_GetNamedConfigParm) {
      char errorBuf[1024] = {0};

      // Try "jsfx_compile_error" parameter
      if (TrackFX_GetNamedConfigParm(track, fxIdx, "jsfx_compile_error", errorBuf,
                                     sizeof(errorBuf))) {
        if (errorBuf[0] != '\0') {
          compileError = errorBuf;
          if (ShowConsoleMsg) {
            char msg[1280];
            snprintf(msg, sizeof(msg), "MAGDA JSFX: Got compile error via jsfx_compile_error: %s\n",
                     errorBuf);
            ShowConsoleMsg(msg);
          }
        }
      }

      // Also try "error" parameter as fallback
      if (compileError.empty()) {
        memset(errorBuf, 0, sizeof(errorBuf));
        if (TrackFX_GetNamedConfigParm(track, fxIdx, "error", errorBuf, sizeof(errorBuf))) {
          if (errorBuf[0] != '\0') {
            compileError = errorBuf;
            if (ShowConsoleMsg) {
              char msg[1280];
              snprintf(msg, sizeof(msg), "MAGDA JSFX: Got compile error via error: %s\n", errorBuf);
              ShowConsoleMsg(msg);
            }
          }
        }
      }

      // Debug: list what parameters are available
      if (ShowConsoleMsg && compileError.empty()) {
        ShowConsoleMsg("MAGDA JSFX: No compile error detected via API\n");
      }
    } else {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA JSFX: TrackFX_GetNamedConfigParm not available\n");
      }
    }

    // If we found an error, report it and trigger AI fix
    if (!compileError.empty()) {
      if (ShowConsoleMsg) {
        char msg[1280];
        snprintf(msg, sizeof(msg), "MAGDA JSFX: Compile error detected: %s\n",
                 compileError.c_str());
        ShowConsoleMsg(msg);
      }

      // Add error to chat as user message to trigger AI fix
      std::string errorMsg = "Fix this compile error: " + compileError;

      JSFXChatMessage errChatMsg;
      errChatMsg.is_user = true;
      errChatMsg.content = errorMsg;
      errChatMsg.has_code_block = false;
      m_chatHistory.push_back(errChatMsg);

      // Trigger AI call to fix it
      SendToAI(errorMsg);
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
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
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
  void (*Main_OnCommand)(int, int) = (void (*)(int, int))m_rec->GetFunc("Main_OnCommand");

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

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
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

std::string MagdaJSFXEditor::TryCompileJSFX(const std::string &code) {
  if (!m_rec) {
    return "REAPER API not available";
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  // Save current state
  std::string savedPath = m_currentFilePath;
  std::string savedCode = m_editorBuffer;
  bool wasModified = m_modified;

  // Create a temp file for compilation test
  std::string effectsFolder = GetEffectsFolder();
  std::string tempPath = effectsFolder + "/MAGDA/_compile_test.jsfx";

  // Ensure MAGDA folder exists
  std::string magdaFolder = effectsFolder + "/MAGDA";
#ifdef _WIN32
  _mkdir(magdaFolder.c_str());
#else
  mkdir(magdaFolder.c_str(), 0755);
#endif

  // Write code to temp file
  std::ofstream tempFile(tempPath);
  if (!tempFile.is_open()) {
    return "Failed to create temp file for compilation";
  }
  tempFile << code;
  tempFile.close();

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA JSFX: Testing compilation...\n");
  }

  // Get REAPER functions
  MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) m_rec->GetFunc("GetSelectedTrack");
  MediaTrack *(*GetTrack)(ReaProject *, int) =
      (MediaTrack * (*)(ReaProject *, int)) m_rec->GetFunc("GetTrack");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))m_rec->GetFunc("TrackFX_AddByName");
  bool (*TrackFX_Delete)(MediaTrack *, int) =
      (bool (*)(MediaTrack *, int))m_rec->GetFunc("TrackFX_Delete");
  bool (*TrackFX_GetNamedConfigParm)(MediaTrack *, int, const char *, char *, int) = (bool (*)(
      MediaTrack *, int, const char *, char *, int))m_rec->GetFunc("TrackFX_GetNamedConfigParm");
  int (*TrackFX_GetCount)(MediaTrack *) = (int (*)(MediaTrack *))m_rec->GetFunc("TrackFX_GetCount");

  if (!TrackFX_AddByName || !GetTrack) {
    std::remove(tempPath.c_str());
    return "Required REAPER API functions not available";
  }

  // Use first track or master track for testing
  MediaTrack *track = GetTrack(nullptr, 0);
  if (!track) {
    // No tracks, try master track
    track = GetTrack(nullptr, -1);
  }

  if (!track) {
    std::remove(tempPath.c_str());
    return "No track available for compilation test";
  }

  // Remember FX count before adding
  int fxCountBefore = TrackFX_GetCount ? TrackFX_GetCount(track) : 0;

  // Try to add the JSFX
  std::string fxName = "JS:MAGDA/_compile_test.jsfx";
  int fxIdx = TrackFX_AddByName(track, fxName.c_str(), false, -1);

  std::string compileError;

  // Get additional REAPER API functions for error detection
  bool (*TrackFX_GetOffline)(MediaTrack *, int) =
      (bool (*)(MediaTrack *, int))m_rec->GetFunc("TrackFX_GetOffline");
  bool (*TrackFX_GetFXName)(MediaTrack *, int, char *, int) =
      (bool (*)(MediaTrack *, int, char *, int))m_rec->GetFunc("TrackFX_GetFXName");
  int (*TrackFX_GetNumParams)(MediaTrack *, int) =
      (int (*)(MediaTrack *, int))m_rec->GetFunc("TrackFX_GetNumParams");

  if (fxIdx >= 0 || (TrackFX_GetCount && TrackFX_GetCount(track) > fxCountBefore)) {
    // FX was added, check for compile errors
    if (fxIdx < 0 && TrackFX_GetCount) {
      fxIdx = TrackFX_GetCount(track) - 1;
    }

    if (fxIdx >= 0) {
      // Method 1: Try TrackFX_GetNamedConfigParm with various error params
      // REAPER uses different param names depending on version
      if (TrackFX_GetNamedConfigParm) {
        char errorBuf[4096] = {0};

        // Try different parameter names that might contain errors
        // "last_error" and "original_name" are most reliable for JSFX
        const char *errorParams[] = {"last_error", "compileerr", "jsfx_error", "error", nullptr};
        for (int i = 0; errorParams[i] && compileError.empty(); i++) {
          memset(errorBuf, 0, sizeof(errorBuf));
          if (TrackFX_GetNamedConfigParm(track, fxIdx, errorParams[i], errorBuf,
                                         sizeof(errorBuf))) {
            if (errorBuf[0] != '\0') {
              compileError = errorBuf;
              if (ShowConsoleMsg) {
                char msg[256];
                snprintf(msg, sizeof(msg), "MAGDA JSFX: Found error via '%s'\n", errorParams[i]);
                ShowConsoleMsg(msg);
              }
            }
          }
        }
      }

      // Method 2: Check FX name for error indicators
      // When JSFX fails to compile, REAPER often shows error in the name
      if (compileError.empty() && TrackFX_GetFXName) {
        char fxNameBuf[512] = {0};
        if (TrackFX_GetFXName(track, fxIdx, fxNameBuf, sizeof(fxNameBuf))) {
          std::string name = fxNameBuf;
          if (ShowConsoleMsg) {
            char msg[512];
            snprintf(msg, sizeof(msg), "MAGDA JSFX: FX name is '%s'\n", fxNameBuf);
            ShowConsoleMsg(msg);
          }

          // Check for error indicators in name
          // REAPER prefixes broken JSFX with "!" or includes error text
          if (name.find("!") == 0 || name.find("JS: !") != std::string::npos) {
            // Extract error from name if present
            size_t colonPos = name.find(": ", 4);
            if (colonPos != std::string::npos) {
              compileError = "JSFX error: " + name.substr(colonPos + 2);
            } else {
              compileError = "JSFX compile error (check syntax)";
            }
          } else if (name.find("error") != std::string::npos ||
                     name.find("Error") != std::string::npos) {
            compileError = "JSFX load error: " + name;
          }
        }
      }

      // Method 3: Check if FX is offline (often indicates compile error)
      if (compileError.empty() && TrackFX_GetOffline) {
        if (TrackFX_GetOffline(track, fxIdx)) {
          compileError = "JSFX is offline - likely compile error (check @init section)";
        }
      }

      // Method 4: Try to get/set a parameter - broken JSFX often fails this
      if (compileError.empty() && TrackFX_GetNumParams) {
        int numParams = TrackFX_GetNumParams(track, fxIdx);
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg), "MAGDA JSFX: FX has %d parameters\n", numParams);
          ShowConsoleMsg(msg);
        }

        // If the code has slider definitions but we got 0 params, something's
        // wrong
        if (numParams == 0 && code.find("slider") != std::string::npos) {
          // Check if there are slider definitions that should create params
          if (code.find("slider1:") != std::string::npos ||
              code.find("slider2:") != std::string::npos) {
            compileError = "JSFX compiled but sliders not created - check slider syntax";
          }
        }
      }
    }

    // Remove the test FX
    if (TrackFX_Delete && fxIdx >= 0) {
      TrackFX_Delete(track, fxIdx);
    }
  } else {
    // FX failed to add - likely a compile error
    compileError = "JSFX failed to load - check syntax";
  }

  // Clean up temp file
  std::remove(tempPath.c_str());

  if (ShowConsoleMsg) {
    if (compileError.empty()) {
      ShowConsoleMsg("MAGDA JSFX: Compilation successful!\n");
    } else {
      char msg[2048];
      snprintf(msg, sizeof(msg), "MAGDA JSFX: Compile error: %s\n", compileError.c_str());
      ShowConsoleMsg(msg);
    }
  }

  return compileError;
}

void MagdaJSFXEditor::RequestFix(size_t messageIndex, const std::string &compileError) {
  if (messageIndex >= m_chatHistory.size()) {
    return;
  }

  JSFXChatMessage &originalMsg = m_chatHistory[messageIndex];
  if (!originalMsg.has_code_block || originalMsg.code_block.empty()) {
    return;
  }

  // Add user message requesting fix
  JSFXChatMessage fixRequest;
  fixRequest.is_user = true;
  fixRequest.has_code_block = false;
  fixRequest.content = "Fix this compile error:\n" + compileError +
                       "\n\nOriginal code that caused the error is above.";
  m_chatHistory.push_back(fixRequest);

  // Add placeholder for AI response
  JSFXChatMessage aiResponse;
  aiResponse.is_user = false;
  aiResponse.has_code_block = false;
  aiResponse.content = "Analyzing error and generating fix...";
  aiResponse.auto_fix_attempt = m_autoFixActive ? m_autoFixAttempt : 0;
  m_chatHistory.push_back(aiResponse);
  size_t aiIndex = m_chatHistory.size() - 1;

  // Update auto-fix message index to track the new response
  if (m_autoFixActive) {
    m_autoFixMessageIndex = aiIndex;
  }

  m_waitingForAI = true;
  m_spinnerStartTime = 0;

  // Build request with error context
  WDL_FastString requestJson;
  requestJson.Set("{\"prompt\":\"Fix this JSFX compile error: ");

  // Escape the error message
  std::string escapedError = compileError;
  for (size_t i = 0; i < escapedError.length(); i++) {
    if (escapedError[i] == '"' || escapedError[i] == '\\') {
      escapedError.insert(i, "\\");
      i++;
    } else if (escapedError[i] == '\n') {
      escapedError.replace(i, 1, "\\n");
      i++;
    }
  }
  requestJson.Append(escapedError.c_str());

  requestJson.Append("\",\"context\":\"");

  // Escape the original code for context
  std::string escapedCode = originalMsg.code_block;
  for (size_t i = 0; i < escapedCode.length(); i++) {
    if (escapedCode[i] == '"' || escapedCode[i] == '\\') {
      escapedCode.insert(i, "\\");
      i++;
    } else if (escapedCode[i] == '\n') {
      escapedCode.replace(i, 1, "\\n");
      i++;
    }
  }
  requestJson.Append(escapedCode.c_str());

  requestJson.Append("\",\"include_description\":false}");

  // Make API call
  std::string requestStr = requestJson.Get();
  bool autoFixActive = m_autoFixActive;

  std::thread([this, requestStr, aiIndex, autoFixActive]() {
    WDL_FastString errorMsg;

    struct JSFXStreamContext {
      MagdaJSFXEditor *editor;
      size_t messageIndex;
      std::string codeBuffer;
      bool autoFixMode;
    } ctx{this, aiIndex, "", autoFixActive};

    auto sseCallback = [](const char *event_json, void *user_data) {
      JSFXStreamContext *ctx = static_cast<JSFXStreamContext *>(user_data);
      if (!ctx || !ctx->editor || ctx->messageIndex >= ctx->editor->m_chatHistory.size()) {
        return;
      }

      MagdaJSFXEditor *editor = ctx->editor;
      wdl_json_parser parser;
      wdl_json_element *root = parser.parse(event_json, (int)strlen(event_json));

      if (!parser.m_err && root) {
        const char *eventType = nullptr;
        if (wdl_json_element *type_elem = root->get_item_by_name("type")) {
          if (type_elem->m_value_string) {
            eventType = type_elem->m_value;
          }
        }

        if (eventType && strcmp(eventType, "chunk") == 0) {
          if (wdl_json_element *chunk_elem = root->get_item_by_name("chunk")) {
            if (chunk_elem->m_value_string) {
              ctx->codeBuffer.append(chunk_elem->m_value);
              JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
              aiMsg.content = ctx->autoFixMode ? "Auto-fixing..." : "Generating fix...";
              aiMsg.code_block = ctx->codeBuffer;
              aiMsg.has_code_block = true;
            }
          }
        } else if (eventType && strcmp(eventType, "done") == 0) {
          std::string finalCode = ctx->codeBuffer;
          if (wdl_json_element *code_elem = root->get_item_by_name("jsfx_code")) {
            if (code_elem->m_value_string) {
              finalCode = code_elem->m_value;
            }
          }

          JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
          aiMsg.code_block = finalCode;
          aiMsg.has_code_block = !finalCode.empty();
          aiMsg.streaming_complete = true;
          aiMsg.content = finalCode.empty() ? "Failed to generate fix." : "Fixed JSFX code:";
          editor->m_waitingForAI = false;

          // If in auto-fix mode, continue the loop
          if (ctx->autoFixMode && editor->m_autoFixActive && !finalCode.empty()) {
            editor->ContinueAutoFix();
          }
        } else if (eventType && strcmp(eventType, "error") == 0) {
          const char *msg = "Error generating fix";
          if (wdl_json_element *msg_elem = root->get_item_by_name("message")) {
            if (msg_elem->m_value_string) {
              msg = msg_elem->m_value;
            }
          }
          JSFXChatMessage &aiMsg = editor->m_chatHistory[ctx->messageIndex];
          aiMsg.content = std::string("Error: ") + msg;
          aiMsg.has_code_block = false;
          editor->m_waitingForAI = false;

          // Stop auto-fix on error
          if (ctx->autoFixMode) {
            editor->StopAutoFix();
          }
        }
      }
    };

    extern MagdaHTTPClient s_jsfxHttpClient;

    // Set backend URL from settings
    const char *backendUrl = MagdaImGuiLogin::GetBackendURL();
    if (backendUrl && backendUrl[0]) {
      s_jsfxHttpClient.SetBackendURL(backendUrl);
    }

    // Only set JWT token if auth is required (Gateway mode)
    extern MagdaImGuiLogin *g_imguiLogin;
    if (g_imguiLogin && g_imguiLogin->GetAuthMode() == AuthMode::Gateway) {
      const char *token = MagdaImGuiLogin::GetStoredToken();
      if (token && token[0]) {
        s_jsfxHttpClient.SetJWTToken(token);
      }
    } else {
      s_jsfxHttpClient.SetJWTToken(nullptr);
    }

    s_jsfxHttpClient.SendPOSTStream("/api/v1/jsfx/generate/stream", requestStr.c_str(), sseCallback,
                                    &ctx, errorMsg, 180);
  }).detach();
}

void MagdaJSFXEditor::StartAutoFix(size_t messageIndex) {
  if (messageIndex >= m_chatHistory.size()) {
    return;
  }

  JSFXChatMessage &msg = m_chatHistory[messageIndex];
  if (!msg.has_code_block || msg.code_block.empty()) {
    return;
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA JSFX: Starting auto-fix loop...\n");
  }

  m_autoFixActive = true;
  m_autoFixMessageIndex = messageIndex;
  m_autoFixAttempt = 1;

  // First, compile the code to get the error
  std::string error = TryCompileJSFX(msg.code_block);

  if (error.empty()) {
    // Already compiles! Mark success and stop
    msg.compile_error.clear();
    msg.compile_checked = true;
    m_autoFixActive = false;

    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Code already compiles successfully!\n");
    }
    return;
  }

  // Store the error and request a fix
  msg.compile_error = error;
  msg.compile_checked = true;

  if (ShowConsoleMsg) {
    char logMsg[512];
    snprintf(logMsg, sizeof(logMsg), "MAGDA JSFX: Auto-fix attempt %d - Error: %s\n",
             m_autoFixAttempt, error.c_str());
    ShowConsoleMsg(logMsg);
  }

  // Request AI to fix
  RequestFix(messageIndex, error);
}

void MagdaJSFXEditor::ContinueAutoFix() {
  if (!m_autoFixActive) {
    return;
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");

  // Get the latest AI response (which should have the fixed code)
  if (m_autoFixMessageIndex >= m_chatHistory.size()) {
    StopAutoFix();
    return;
  }

  JSFXChatMessage &fixedMsg = m_chatHistory[m_autoFixMessageIndex];
  if (!fixedMsg.has_code_block || fixedMsg.code_block.empty()) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Auto-fix failed - no code generated\n");
    }
    StopAutoFix();
    return;
  }

  // Try to compile the fixed code
  std::string error = TryCompileJSFX(fixedMsg.code_block);

  fixedMsg.compile_error = error;
  fixedMsg.compile_checked = true;

  if (error.empty()) {
    // Success!
    if (ShowConsoleMsg) {
      char logMsg[256];
      snprintf(logMsg, sizeof(logMsg), "MAGDA JSFX: Auto-fix SUCCESS after %d attempt(s)!\n",
               m_autoFixAttempt);
      ShowConsoleMsg(logMsg);
    }
    StopAutoFix();
    return;
  }

  // Still has errors - check if we should retry
  m_autoFixAttempt++;

  if (m_autoFixAttempt > MAX_AUTO_FIX_ATTEMPTS) {
    if (ShowConsoleMsg) {
      char logMsg[256];
      snprintf(logMsg, sizeof(logMsg),
               "MAGDA JSFX: Auto-fix gave up after %d attempts. Last error: %s\n",
               MAX_AUTO_FIX_ATTEMPTS, error.c_str());
      ShowConsoleMsg(logMsg);
    }
    StopAutoFix();
    return;
  }

  if (ShowConsoleMsg) {
    char logMsg[512];
    snprintf(logMsg, sizeof(logMsg), "MAGDA JSFX: Auto-fix attempt %d/%d - Error: %s\n",
             m_autoFixAttempt, MAX_AUTO_FIX_ATTEMPTS, error.c_str());
    ShowConsoleMsg(logMsg);
  }

  // Request another fix
  RequestFix(m_autoFixMessageIndex, error);
}

void MagdaJSFXEditor::StopAutoFix() {
  m_autoFixActive = false;
  m_autoFixAttempt = 0;

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA JSFX: Auto-fix stopped\n");
  }
}

void MagdaJSFXEditor::RenderSaveAsDialog() {
  if (!m_ctx)
    return;

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
    m_ImGui_InputText(m_ctx, "##saveas_filename", m_saveAsFilename, sizeof(m_saveAsFilename),
                      &inputFlags, nullptr);

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
          RefreshFileList();  // Update file list to show new file
          RefreshFXBrowser(); // Update REAPER's FX browser
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

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA JSFX: Created folder %s\n", name.c_str());
    ShowConsoleMsg(msg);
  }
}
