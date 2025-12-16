#include "magda_imgui_chat.h"
#include "magda_plugin_scanner.h"
#include <algorithm>
#include <cstring>

// g_rec is needed for debug logging
extern reaper_plugin_info_t *g_rec;

// ReaImGui constants
namespace ImGuiCond {
constexpr int FirstUseEver = 1 << 2;
} // namespace ImGuiCond

namespace ImGuiWindowFlags {
constexpr int NoCollapse = 1 << 5;
constexpr int AlwaysVerticalScrollbar = 1 << 14;
} // namespace ImGuiWindowFlags

namespace ImGuiInputTextFlags {
constexpr int EnterReturnsTrue = 1 << 5;
} // namespace ImGuiInputTextFlags

namespace ImGuiCol {
constexpr int ChildBg = 3;
constexpr int Button = 21;
} // namespace ImGuiCol

namespace ImGuiKey {
constexpr int Enter = 525;
constexpr int Escape = 527;
constexpr int UpArrow = 516;
constexpr int DownArrow = 517;
constexpr int Tab = 512;
} // namespace ImGuiKey

namespace ImGuiTableFlags {
constexpr int Resizable = 1 << 1;
constexpr int BordersInnerV = 1 << 8;
} // namespace ImGuiTableFlags

namespace ImGuiTableColumnFlags {
constexpr int WidthStretch = 1 << 1;
} // namespace ImGuiTableColumnFlags

// Colors (ABGR format for ReaImGui)
namespace Colors {
constexpr int UserBg = 0xFF3D5A80;      // Blue-ish
constexpr int AssistantBg = 0xFF2D3142; // Dark gray
constexpr int HeaderText = 0xFFFFCC88;  // Gold
constexpr int StatusGreen = 0xFF88FF88;
constexpr int StatusRed = 0xFF8888FF;
constexpr int StatusYellow = 0xFF88FFFF;
constexpr int GrayText = 0xFF888888;
} // namespace Colors

MagdaImGuiChat::MagdaImGuiChat() {
  // Zero out function pointers
  m_ImGui_CreateContext = nullptr;
  m_ImGui_ConfigFlags_DockingEnable = nullptr;
  m_ImGui_Begin = nullptr;
  m_ImGui_End = nullptr;
  m_ImGui_SetNextWindowSize = nullptr;
  m_ImGui_Text = nullptr;
  m_ImGui_TextColored = nullptr;
  m_ImGui_TextWrapped = nullptr;
  m_ImGui_InputText = nullptr;
  m_ImGui_Button = nullptr;
  m_ImGui_SameLine = nullptr;
  m_ImGui_Separator = nullptr;
  m_ImGui_BeginChild = nullptr;
  m_ImGui_EndChild = nullptr;
  m_ImGui_BeginPopup = nullptr;
  m_ImGui_EndPopup = nullptr;
  m_ImGui_OpenPopup = nullptr;
  m_ImGui_CloseCurrentPopup = nullptr;
  m_ImGui_Selectable = nullptr;
  m_ImGui_IsWindowAppearing = nullptr;
  m_ImGui_SetKeyboardFocusHere = nullptr;
  m_ImGui_GetScrollY = nullptr;
  m_ImGui_GetScrollMaxY = nullptr;
  m_ImGui_SetScrollHereY = nullptr;
  m_ImGui_GetKeyMods = nullptr;
  m_ImGui_IsKeyPressed = nullptr;
  m_ImGui_PushStyleColor = nullptr;
  m_ImGui_PopStyleColor = nullptr;
  m_ImGui_BeginPopupContextWindow = nullptr;
  m_ImGui_IsWindowDocked = nullptr;
  m_ImGui_SetNextWindowDockID = nullptr;
  m_ImGui_MenuItem = nullptr;
  m_ImGui_BeginTable = nullptr;
  m_ImGui_EndTable = nullptr;
  m_ImGui_TableNextRow = nullptr;
  m_ImGui_TableNextColumn = nullptr;
  m_ImGui_TableSetupColumn = nullptr;
  m_ImGui_TableHeadersRow = nullptr;
  m_ImGui_GetContentRegionAvail = nullptr;
  m_ImGui_Dummy = nullptr;
}

MagdaImGuiChat::~MagdaImGuiChat() { m_ctx = nullptr; }

bool MagdaImGuiChat::Initialize(reaper_plugin_info_t *rec) {
  if (!rec) {
    return false;
  }

  // Get ShowConsoleMsg for logging
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))rec->GetFunc("ShowConsoleMsg");

#define LOAD_IMGUI_FUNC(name, sig)                                             \
  m_##name = (sig)rec->GetFunc(#name);                                         \
  if (!m_##name) {                                                             \
    if (ShowConsoleMsg) {                                                      \
      ShowConsoleMsg("MAGDA ImGui: Failed to load " #name "\n");               \
    }                                                                          \
    return false;                                                              \
  }

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA ImGui: Loading ReaImGui functions...\n");
  }

  // Load essential ReaImGui functions
  LOAD_IMGUI_FUNC(ImGui_CreateContext, void *(*)(const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_ConfigFlags_DockingEnable, int (*)());
  LOAD_IMGUI_FUNC(ImGui_Begin, bool (*)(void *, const char *, bool *, int *));
  LOAD_IMGUI_FUNC(ImGui_End, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetNextWindowSize,
                  void (*)(void *, double, double, int *));
  LOAD_IMGUI_FUNC(ImGui_Text, void (*)(void *, const char *));
  LOAD_IMGUI_FUNC(ImGui_TextColored, void (*)(void *, int, const char *));
  LOAD_IMGUI_FUNC(ImGui_TextWrapped, void (*)(void *, const char *));
  LOAD_IMGUI_FUNC(ImGui_InputText,
                  bool (*)(void *, const char *, char *, int, int *, void *));
  LOAD_IMGUI_FUNC(ImGui_Button,
                  bool (*)(void *, const char *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_SameLine, void (*)(void *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_Separator, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_BeginChild, bool (*)(void *, const char *, double *,
                                             double *, int *, int *));
  LOAD_IMGUI_FUNC(ImGui_EndChild, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_BeginPopup, bool (*)(void *, const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_EndPopup, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_OpenPopup, void (*)(void *, const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_CloseCurrentPopup, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_Selectable, bool (*)(void *, const char *, bool *,
                                             int *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_IsWindowAppearing, bool (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetKeyboardFocusHere, void (*)(void *, int *));
  LOAD_IMGUI_FUNC(ImGui_GetScrollY, double (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_GetScrollMaxY, double (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetScrollHereY, void (*)(void *, double *));
  LOAD_IMGUI_FUNC(ImGui_GetKeyMods, int (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_IsKeyPressed, bool (*)(void *, int, bool *));
  LOAD_IMGUI_FUNC(ImGui_PushStyleColor, void (*)(void *, int, int));
  LOAD_IMGUI_FUNC(ImGui_PopStyleColor, void (*)(void *, int *));
  LOAD_IMGUI_FUNC(ImGui_BeginPopupContextWindow,
                  bool (*)(void *, const char *, int *));
  LOAD_IMGUI_FUNC(ImGui_IsWindowDocked, bool (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_SetNextWindowDockID, void (*)(void *, int, int *));
  LOAD_IMGUI_FUNC(ImGui_MenuItem,
                  bool (*)(void *, const char *, const char *, bool *, bool *));
  LOAD_IMGUI_FUNC(ImGui_BeginTable, bool (*)(void *, const char *, int, int *,
                                             double *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_EndTable, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_TableNextRow, void (*)(void *, int *, double *));
  LOAD_IMGUI_FUNC(ImGui_TableNextColumn, bool (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_TableSetupColumn,
                  void (*)(void *, const char *, int *, double *, int *));
  LOAD_IMGUI_FUNC(ImGui_TableHeadersRow, void (*)(void *));
  LOAD_IMGUI_FUNC(ImGui_GetContentRegionAvail,
                  void (*)(void *, double *, double *));
  LOAD_IMGUI_FUNC(ImGui_Dummy, void (*)(void *, double, double));

#undef LOAD_IMGUI_FUNC

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA ImGui: All functions loaded successfully\n");
  }

  m_available = true;
  return true;
}

void MagdaImGuiChat::Show() { m_visible = true; }

void MagdaImGuiChat::Hide() { m_visible = false; }

void MagdaImGuiChat::Toggle() {
  m_visible = !m_visible;
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg(m_visible ? "MAGDA ImGui: Showing chat\n"
                               : "MAGDA ImGui: Hiding chat\n");
    }
  }
}

void MagdaImGuiChat::Render() {
  if (!m_available || !m_visible) {
    return;
  }

  // Log helper
  void (*ShowConsoleMsg)(const char *msg) = nullptr;
  if (g_rec) {
    ShowConsoleMsg = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  }

  // Create context on first use - ReaImGui contexts stay valid as long as used
  // each frame
  if (!m_ctx) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA ImGui: Creating context...\n");
    }
    int configFlags = m_ImGui_ConfigFlags_DockingEnable();
    m_ctx = m_ImGui_CreateContext("MAGDA", &configFlags);
    if (!m_ctx) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA ImGui: Failed to create context!\n");
      }
      return;
    }
    if (ShowConsoleMsg) {
      char buf[128];
      snprintf(buf, sizeof(buf), "MAGDA ImGui: Context created: %p\n", m_ctx);
      ShowConsoleMsg(buf);
    }
  }

  // Set initial window size
  int cond = ImGuiCond::FirstUseEver;
  m_ImGui_SetNextWindowSize(m_ctx, 800, 600, &cond);

  // Apply pending dock ID
  if (m_hasPendingDock) {
    m_ImGui_SetNextWindowDockID(m_ctx, m_pendingDockID, nullptr);
    m_hasPendingDock = false;
  }

  bool open = true;
  int flags = ImGuiWindowFlags::NoCollapse;
  bool visible = m_ImGui_Begin(m_ctx, "MAGDA Chat", &open, &flags);

  // Log Begin result (only first few times)
  static int beginLogCount = 0;
  if (beginLogCount < 5 && ShowConsoleMsg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "MAGDA ImGui: Begin() visible=%d, open=%d\n",
             visible, open);
    ShowConsoleMsg(buf);
    beginLogCount++;
  }

  // Right-click context menu for dock/undock
  if (m_ImGui_BeginPopupContextWindow(m_ctx, "##window_context", nullptr)) {
    bool isDocked = m_ImGui_IsWindowDocked(m_ctx);

    if (isDocked) {
      if (m_ImGui_MenuItem(m_ctx, "Undock Window", nullptr, nullptr, nullptr)) {
        m_pendingDockID = 0;
        m_hasPendingDock = true;
      }
    } else {
      m_ImGui_Text(m_ctx, "Dock to:");
      if (m_ImGui_MenuItem(m_ctx, "Bottom Docker", nullptr, nullptr, nullptr)) {
        m_pendingDockID = 1;
        m_hasPendingDock = true;
      }
      if (m_ImGui_MenuItem(m_ctx, "Left Docker", nullptr, nullptr, nullptr)) {
        m_pendingDockID = 2;
        m_hasPendingDock = true;
      }
      if (m_ImGui_MenuItem(m_ctx, "Right Docker", nullptr, nullptr, nullptr)) {
        m_pendingDockID = 3;
        m_hasPendingDock = true;
      }
    }

    m_ImGui_Separator(m_ctx);

    if (m_ImGui_MenuItem(m_ctx, "Close", nullptr, nullptr, nullptr)) {
      m_visible = false;
    }

    m_ImGui_EndPopup(m_ctx);
  }

  // Only render content if window is visible (not collapsed)
  if (visible) {
    // Header
    m_ImGui_TextColored(m_ctx, Colors::HeaderText,
                        "MAGDA - AI Music Production Assistant");
    m_ImGui_Separator(m_ctx);

    // Input area
    int inputFlags = ImGuiInputTextFlags::EnterReturnsTrue;
    bool submitted =
        m_ImGui_InputText(m_ctx, "##input", m_inputBuffer,
                          sizeof(m_inputBuffer), &inputFlags, nullptr);

    double spacing = 5;
    double zero = 0;
    m_ImGui_SameLine(m_ctx, &zero, &spacing);

    if (m_ImGui_Button(m_ctx, m_busy ? "..." : "Send", nullptr, nullptr) ||
        submitted) {
      if (!m_busy && strlen(m_inputBuffer) > 0 && m_onSend) {
        std::string msg = m_inputBuffer;
        AddUserMessage(msg);
        m_onSend(msg);
        m_inputBuffer[0] = '\0';
      }
    }

    m_ImGui_Separator(m_ctx);

    // Simple message display (no columns for now)
    double childW = 0;
    double childH = -30; // Leave room for footer
    int childFlags = 0;
    int windowFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

    if (m_ImGui_BeginChild(m_ctx, "##messages", &childW, &childH, &childFlags,
                           &windowFlags)) {
      for (const auto &msg : m_history) {
        int bgColor = msg.is_user ? Colors::UserBg : Colors::AssistantBg;
        m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, bgColor);

        const char *label = msg.is_user ? "You: " : "MAGDA: ";
        m_ImGui_TextColored(m_ctx, 0xFF88AACC, label);
        double labelOffset = 0;
        double labelSpacing = 0;
        m_ImGui_SameLine(m_ctx, &labelOffset, &labelSpacing);
        m_ImGui_TextWrapped(m_ctx, msg.content.c_str());

        int popCount = 1;
        m_ImGui_PopStyleColor(m_ctx, &popCount);
      }

      if (m_scrollToBottom) {
        double ratio = 1.0;
        m_ImGui_SetScrollHereY(m_ctx, &ratio);
        m_scrollToBottom = false;
      }
    }
    m_ImGui_EndChild(m_ctx);

    // Footer
    m_ImGui_Separator(m_ctx);
    m_ImGui_TextColored(m_ctx, Colors::GrayText, "Status: ");
    m_ImGui_SameLine(m_ctx, &zero, &zero);
    m_ImGui_TextColored(m_ctx, Colors::StatusGreen, m_apiStatus.c_str());
  }

  m_ImGui_End(m_ctx);

  // Handle window close
  if (!open) {
    m_visible = false;
  }
}

void MagdaImGuiChat::RenderHeader() {
  m_ImGui_TextColored(m_ctx, Colors::HeaderText,
                      "MAGDA - AI Music Production Assistant");
}

void MagdaImGuiChat::RenderMainContent() {
  double availW, availH;
  m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);
  double contentHeight = availH - 30;
  if (contentHeight < 100)
    contentHeight = 100;

  int tableFlags = ImGuiTableFlags::Resizable | ImGuiTableFlags::BordersInnerV;
  double outerSizeW = 0;
  double outerSizeH = contentHeight;
  double innerWidth = 0;

  if (m_ImGui_BeginTable(m_ctx, "##main_layout", 3, &tableFlags, &outerSizeW,
                         &outerSizeH, &innerWidth)) {
    int stretchFlags = ImGuiTableColumnFlags::WidthStretch;
    double col1Weight = 1.0;
    double col2Weight = 1.0;
    double col3Weight = 0.6;
    m_ImGui_TableSetupColumn(m_ctx, "REQUEST", &stretchFlags, &col1Weight,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "RESPONSE", &stretchFlags, &col2Weight,
                             nullptr);
    m_ImGui_TableSetupColumn(m_ctx, "CONTROLS", &stretchFlags, &col3Weight,
                             nullptr);
    m_ImGui_TableHeadersRow(m_ctx);

    m_ImGui_TableNextRow(m_ctx, nullptr, nullptr);

    m_ImGui_TableNextColumn(m_ctx);
    RenderRequestColumn();

    m_ImGui_TableNextColumn(m_ctx);
    RenderResponseColumn();

    m_ImGui_TableNextColumn(m_ctx);
    RenderControlsColumn();

    m_ImGui_EndTable(m_ctx);
  }
}

void MagdaImGuiChat::RenderRequestColumn() {
  double zero = 0.0;
  double negSpace = -5.0;
  int childFlags = 0;
  int windowFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

  if (m_ImGui_BeginChild(m_ctx, "##request_scroll", &zero, &negSpace,
                         &childFlags, &windowFlags)) {
    for (const auto &msg : m_history) {
      if (!msg.is_user)
        continue;

      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, Colors::UserBg);
      std::string msgId = "##req_" + std::to_string(&msg - m_history.data());
      int msgChildFlags = 1;
      int msgWindowFlags = 0;
      if (m_ImGui_BeginChild(m_ctx, msgId.c_str(), &zero, &zero, &msgChildFlags,
                             &msgWindowFlags)) {
        RenderMessageWithHighlighting(msg.content);
      }
      m_ImGui_EndChild(m_ctx);
      int popCount = 1;
      m_ImGui_PopStyleColor(m_ctx, &popCount);

      m_ImGui_Dummy(m_ctx, 0, 5);
    }

    if (m_scrollToBottom) {
      double ratio = 1.0;
      m_ImGui_SetScrollHereY(m_ctx, &ratio);
    }
  }
  m_ImGui_EndChild(m_ctx);
}

void MagdaImGuiChat::RenderResponseColumn() {
  double zero = 0.0;
  double negSpace = -5.0;
  int childFlags = 0;
  int windowFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

  if (m_ImGui_BeginChild(m_ctx, "##response_scroll", &zero, &negSpace,
                         &childFlags, &windowFlags)) {
    for (const auto &msg : m_history) {
      if (msg.is_user)
        continue;

      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, Colors::AssistantBg);
      std::string msgId = "##resp_" + std::to_string(&msg - m_history.data());
      int msgChildFlags = 1;
      int msgWindowFlags = 0;
      if (m_ImGui_BeginChild(m_ctx, msgId.c_str(), &zero, &zero, &msgChildFlags,
                             &msgWindowFlags)) {
        RenderMessageWithHighlighting(msg.content);
      }
      m_ImGui_EndChild(m_ctx);
      int popCount = 1;
      m_ImGui_PopStyleColor(m_ctx, &popCount);

      m_ImGui_Dummy(m_ctx, 0, 5);
    }

    if (!m_streamingBuffer.empty()) {
      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, Colors::AssistantBg);
      int streamChildFlags = 1;
      int streamWindowFlags = 0;
      if (m_ImGui_BeginChild(m_ctx, "##streaming", &zero, &zero,
                             &streamChildFlags, &streamWindowFlags)) {
        m_ImGui_TextWrapped(m_ctx, m_streamingBuffer.c_str());
      }
      m_ImGui_EndChild(m_ctx);
      int popCount = 1;
      m_ImGui_PopStyleColor(m_ctx, &popCount);
    }

    if (m_scrollToBottom) {
      double ratio = 1.0;
      m_ImGui_SetScrollHereY(m_ctx, &ratio);
      m_scrollToBottom = false;
    }
  }
  m_ImGui_EndChild(m_ctx);
}

void MagdaImGuiChat::RenderControlsColumn() {
  m_ImGui_Text(m_ctx, "Macro Actions:");
  m_ImGui_Dummy(m_ctx, 0, 5);

  double btnWidth = -1.0;
  double btnHeight = 28.0;

  if (m_ImGui_Button(m_ctx, "Mix Analysis", &btnWidth, &btnHeight)) {
    if (m_onSend) {
      m_onSend("Analyze my mix and suggest improvements");
    }
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  if (m_ImGui_Button(m_ctx, "Master Analysis", &btnWidth, &btnHeight)) {
    if (m_onSend) {
      m_onSend("Analyze the master bus and suggest mastering adjustments");
    }
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  if (m_ImGui_Button(m_ctx, "Gain Staging", &btnWidth, &btnHeight)) {
    if (m_onSend) {
      m_onSend("Check gain staging across all tracks");
    }
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  if (m_ImGui_Button(m_ctx, "Housekeeping", &btnWidth, &btnHeight)) {
    if (m_onSend) {
      m_onSend("Clean up and organize this project");
    }
  }

  m_ImGui_Separator(m_ctx);
  m_ImGui_Dummy(m_ctx, 0, 10);

  m_ImGui_Text(m_ctx, "Preferences:");
  m_ImGui_Dummy(m_ctx, 0, 5);

  if (m_ImGui_Button(m_ctx, "Plugin Aliases...", &btnWidth, &btnHeight)) {
    // TODO: Open plugin aliases window
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  if (m_ImGui_Button(m_ctx, "Drum Mappings...", &btnWidth, &btnHeight)) {
    // TODO: Open drum mappings window
  }
}

void MagdaImGuiChat::RenderFooter() {
  m_ImGui_TextColored(m_ctx, Colors::GrayText, "Status: ");
  double offset = 0;
  double spacing = 0;
  m_ImGui_SameLine(m_ctx, &offset, &spacing);

  int statusColor = Colors::StatusGreen;
  if (m_apiStatus.find("Error") != std::string::npos ||
      m_apiStatus.find("Disconnected") != std::string::npos) {
    statusColor = Colors::StatusRed;
  } else if (m_apiStatus.find("Checking") != std::string::npos) {
    statusColor = Colors::StatusYellow;
  }

  m_ImGui_TextColored(m_ctx, statusColor, m_apiStatus.c_str());
}

void MagdaImGuiChat::RenderInputArea() {
  int flags = ImGuiInputTextFlags::EnterReturnsTrue;

  bool submitted = m_ImGui_InputText(m_ctx, "##input", m_inputBuffer,
                                     sizeof(m_inputBuffer), &flags, nullptr);

  DetectAtTrigger();

  if (m_showAutocomplete && !m_suggestions.empty()) {
    bool repeatTrue = true;
    bool repeatFalse = false;
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::UpArrow, &repeatTrue)) {
      m_autocompleteIndex = (m_autocompleteIndex - 1 + m_suggestions.size()) %
                            m_suggestions.size();
    }
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::DownArrow, &repeatTrue)) {
      m_autocompleteIndex = (m_autocompleteIndex + 1) % m_suggestions.size();
    }
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Tab, &repeatFalse) ||
        m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Enter, &repeatFalse)) {
      InsertCompletion(m_suggestions[m_autocompleteIndex].alias);
      m_showAutocomplete = false;
      return;
    }
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Escape, &repeatFalse)) {
      m_showAutocomplete = false;
    }
  }

  double offset = 0;
  double spacing = 5;
  m_ImGui_SameLine(m_ctx, &offset, &spacing);

  bool canSend = !m_busy && strlen(m_inputBuffer) > 0;
  if (!canSend) {
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF555555);
  }

  if (m_ImGui_Button(m_ctx, m_busy ? "..." : "Send", nullptr, nullptr) ||
      submitted) {
    if (canSend && m_onSend) {
      std::string msg = m_inputBuffer;
      AddUserMessage(msg);
      m_onSend(msg);
      m_inputBuffer[0] = '\0';
      m_showAutocomplete = false;
    }
  }

  if (!canSend) {
    int popCount = 1;
    m_ImGui_PopStyleColor(m_ctx, &popCount);
  }
}

void MagdaImGuiChat::RenderAutocompletePopup() {
  m_ImGui_OpenPopup(m_ctx, "##autocomplete", nullptr);

  if (m_ImGui_BeginPopup(m_ctx, "##autocomplete", nullptr)) {
    int idx = 0;
    for (const auto &suggestion : m_suggestions) {
      bool selected = (idx == m_autocompleteIndex);
      std::string label =
          "@" + suggestion.alias + " (" + suggestion.plugin_name + ")";

      if (m_ImGui_Selectable(m_ctx, label.c_str(), &selected, nullptr, nullptr,
                             nullptr)) {
        InsertCompletion(suggestion.alias);
        m_showAutocomplete = false;
        m_ImGui_CloseCurrentPopup(m_ctx);
      }
      idx++;

      if (idx >= 10)
        break;
    }
    m_ImGui_EndPopup(m_ctx);
  }
}

void MagdaImGuiChat::RenderMessageWithHighlighting(const std::string &content) {
  m_ImGui_TextWrapped(m_ctx, content.c_str());
}

void MagdaImGuiChat::DetectAtTrigger() {
  std::string input = m_inputBuffer;

  size_t atPos = input.rfind('@');

  if (atPos == std::string::npos) {
    m_showAutocomplete = false;
    m_atPosition = std::string::npos;
    return;
  }

  if (atPos > 0 && input[atPos - 1] != ' ') {
    m_showAutocomplete = false;
    return;
  }

  m_atPosition = atPos;
  m_autocompletePrefix = input.substr(atPos + 1);

  size_t spacePos = m_autocompletePrefix.find(' ');
  if (spacePos != std::string::npos) {
    m_showAutocomplete = false;
    return;
  }

  UpdateAutocompleteSuggestions();
  m_showAutocomplete = !m_suggestions.empty();
  m_autocompleteIndex = 0;
}

void MagdaImGuiChat::UpdateAutocompleteSuggestions() {
  m_suggestions.clear();

  if (!m_pluginScanner) {
    return;
  }

  std::string query = m_autocompletePrefix;
  std::transform(query.begin(), query.end(), query.begin(), ::tolower);

  const auto &aliases = m_pluginScanner->GetAliases();

  for (const auto &pair : aliases) {
    const std::string &alias = pair.first;
    const std::string &pluginName = pair.second;

    std::string aliasLower = alias;
    std::transform(aliasLower.begin(), aliasLower.end(), aliasLower.begin(),
                   ::tolower);

    if (query.empty() || aliasLower.find(query) == 0) {
      AutocompleteSuggestion suggestion;
      suggestion.alias = alias;
      suggestion.plugin_name = pluginName;
      suggestion.plugin_type = "fx"; // TODO: detect instrument vs fx
      m_suggestions.push_back(suggestion);
    }
  }

  std::sort(m_suggestions.begin(), m_suggestions.end(),
            [&query](const AutocompleteSuggestion &a,
                     const AutocompleteSuggestion &b) {
              bool aStartsWith = a.alias.find(query) == 0;
              bool bStartsWith = b.alias.find(query) == 0;
              if (aStartsWith != bStartsWith) {
                return aStartsWith;
              }
              return a.alias < b.alias;
            });
}

void MagdaImGuiChat::InsertCompletion(const std::string &alias) {
  if (m_atPosition == std::string::npos) {
    return;
  }

  std::string input = m_inputBuffer;
  std::string before = input.substr(0, m_atPosition);
  std::string completion = "@" + alias + " ";
  std::string newInput = before + completion;

  strncpy(m_inputBuffer, newInput.c_str(), sizeof(m_inputBuffer) - 1);
  m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';

  m_atPosition = std::string::npos;
}

void MagdaImGuiChat::AddUserMessage(const std::string &msg) {
  ChatMessage message;
  message.content = msg;
  message.is_user = true;
  m_history.push_back(message);
  m_scrollToBottom = true;
}

void MagdaImGuiChat::AddAssistantMessage(const std::string &msg) {
  ChatMessage message;
  message.content = msg;
  message.is_user = false;
  m_history.push_back(message);
  m_scrollToBottom = true;
}

void MagdaImGuiChat::AppendStreamingText(const std::string &chunk) {
  m_streamingBuffer += chunk;
  m_scrollToBottom = true;
}

void MagdaImGuiChat::ClearStreamingBuffer() {
  if (!m_streamingBuffer.empty()) {
    AddAssistantMessage(m_streamingBuffer);
    m_streamingBuffer.clear();
  }
}
