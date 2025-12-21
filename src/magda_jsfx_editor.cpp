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

  // Sort: directories first (except ..), then alphabetically
  std::sort(m_files.begin(), m_files.end(),
            [](const JSFXFileEntry &a, const JSFXFileEntry &b) {
              // Keep ".." at the top
              if (a.name == "..") return true;
              if (b.name == "..") return false;
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
    if (m_currentFilePath.empty()) {
      // Save as new file in current folder
      std::string path = m_currentFolder + "/" + m_currentFileName;
      if (m_currentFileName.empty() || m_currentFileName == "untitled.jsfx") {
        path = m_currentFolder + "/untitled.jsfx";
      }
      m_currentFilePath = path;
    }
    SaveCurrentFile();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Refresh", nullptr, nullptr)) {
    RefreshFileList();
  }

  m_ImGui_SameLine(m_ctx, &zero, &spacing);

  if (m_ImGui_Button(m_ctx, "Recompile", nullptr, nullptr)) {
    // Save and refresh any loaded instances
    if (m_modified) {
      SaveCurrentFile();
    }
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Recompiled\n");
    }
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
      // Parse response JSON to extract jsfx_code
      std::string responseStr = response.Get();
      
      // Simple JSON parsing for jsfx_code field
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
        
        // Unescape the code
        std::string unescaped;
        for (size_t i = 0; i < code.length(); i++) {
          if (code[i] == '\\' && i + 1 < code.length()) {
            switch (code[i + 1]) {
            case 'n': unescaped += '\n'; i++; break;
            case 't': unescaped += '\t'; i++; break;
            case '"': unescaped += '"'; i++; break;
            case '\\': unescaped += '\\'; i++; break;
            default: unescaped += code[i]; break;
            }
          } else {
            unescaped += code[i];
          }
        }
        
        aiMsg.content = "Generated JSFX code:";
        aiMsg.code_block = unescaped;
        aiMsg.has_code_block = true;
      } else {
        aiMsg.content = "JSFX generated but couldn't parse response.";
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
  
  // Log to console
  if (m_rec) {
    void (*ShowConsoleMsg)(const char *) =
        (void (*)(const char *))m_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA JSFX: Applied AI-generated code to editor\n");
    }
  }
}

void MagdaJSFXEditor::ProcessAIResponse(const std::string &response) {
  // Parse response and extract code blocks
  // TODO: Implement proper parsing
}

