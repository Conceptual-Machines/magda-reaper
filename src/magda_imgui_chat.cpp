#include "magda_imgui_chat.h"
#include "../WDL/WDL/jsonparse.h"
#include "magda_actions.h"
#include "magda_api_client.h"
#include "magda_bounce_workflow.h"
#include "magda_login_window.h"
#include "magda_plugin_scanner.h"
#include "magda_settings_window.h"
#include "magda_state.h"
#include <algorithm>
#include <cstring>
#include <ctime>

// Static HTTP client instance
static MagdaHTTPClient s_httpClient;

// g_rec is needed for debug logging
extern reaper_plugin_info_t *g_rec;

// Forward declaration
extern void magdaAction(int command_id, int flag);

// Command ID for mix analysis (defined in main.cpp, dynamically allocated)
extern int g_cmdMixAnalyze;

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
constexpr int Text = 0;
constexpr int WindowBg = 2;
constexpr int ChildBg = 3;
constexpr int Border = 5;
constexpr int FrameBg = 7; // Input field background
constexpr int FrameBgHovered = 8;
constexpr int FrameBgActive = 9;
constexpr int TitleBg = 10;
constexpr int TitleBgActive = 11;
constexpr int Button = 21;
constexpr int ButtonHovered = 22;
constexpr int ButtonActive = 23;
constexpr int Header = 24; // Column headers
constexpr int HeaderHovered = 25;
constexpr int HeaderActive = 26;
constexpr int Separator = 27;
constexpr int ScrollbarBg = 14;
constexpr int ScrollbarGrab = 15;
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

// Theme colors - format is 0xRRGGBBAA
// Helper macro: RGBA(r,g,b) creates 0xRRGGBBFF
#define THEME_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xFF)

struct ThemeColors {
  int windowBg = THEME_RGBA(0x3C, 0x3C, 0x3C);     // Dark grey
  int childBg = THEME_RGBA(0x2D, 0x2D, 0x2D);      // Slightly darker panels
  int textAreaBg = THEME_RGBA(0x1A, 0x1A, 0x1A);   // Near-black for text areas
  int headerText = THEME_RGBA(0xE0, 0xE0, 0xE0);   // White headers
  int normalText = THEME_RGBA(0xD0, 0xD0, 0xD0);   // Light grey text
  int dimText = THEME_RGBA(0x90, 0x90, 0x90);      // Dimmed grey
  int accent = THEME_RGBA(0x52, 0x94, 0xE2);       // Blue accent
  int userBg = THEME_RGBA(0x2D, 0x2D, 0x2D);       // Dark grey
  int assistantBg = THEME_RGBA(0x35, 0x35, 0x35);  // Slightly lighter
  int statusGreen = THEME_RGBA(0x88, 0xFF, 0x88);  // Green
  int statusRed = THEME_RGBA(0xFF, 0x66, 0x66);    // Red
  int statusYellow = THEME_RGBA(0xFF, 0xFF, 0x66); // Yellow
  int border = THEME_RGBA(0x50, 0x50, 0x50);       // Grey border
  int buttonBg = THEME_RGBA(0x48, 0x48, 0x48);     // Grey button
  int buttonHover = THEME_RGBA(0x58, 0x58, 0x58);  // Lighter on hover
  int inputBg = THEME_RGBA(0x1E, 0x1E, 0x1E);      // Dark input
};

static ThemeColors g_theme;

// Helper function to extract action summary from response JSON
static std::string ExtractActionSummary(const char *response_json) {
  if (!response_json || !response_json[0]) {
    return "Done";
  }

  wdl_json_parser parser;
  wdl_json_element *root =
      parser.parse(response_json, (int)strlen(response_json));
  if (parser.m_err || !root) {
    return "Done";
  }

  // Look for "actions" array in response
  wdl_json_element *actions = root->get_item_by_name("actions");
  if (!actions || !actions->is_array()) {
    return "Done";
  }

  // Count actions and collect action types
  int action_count = 0;
  std::string action_types;
  std::vector<std::string> unique_types;

  // Iterate through array using enum_item
  int idx = 0;
  wdl_json_element *action = actions->enum_item(idx);
  while (action) {
    action_count++;
    wdl_json_element *action_type = action->get_item_by_name("action");
    if (action_type && action_type->m_value_string) {
      std::string type = action_type->m_value;
      // Simplify action names for display
      if (type.find("create_") == 0) {
        type = type.substr(7); // Remove "create_" prefix
      } else if (type.find("set_") == 0) {
        type = type.substr(4); // Remove "set_" prefix
      } else if (type.find("add_") == 0) {
        type = type.substr(4); // Remove "add_" prefix
      }
      // Only add unique types
      bool found = false;
      for (const auto &t : unique_types) {
        if (t == type) {
          found = true;
          break;
        }
      }
      if (!found && unique_types.size() < 3) {
        unique_types.push_back(type);
      }
    }
    idx++;
    action = actions->enum_item(idx);
  }

  if (action_count == 0) {
    return "Done (no actions)";
  }

  // Build summary string
  std::string summary;
  if (action_count == 1) {
    summary = "Completed 1 action";
  } else {
    summary = "Completed " + std::to_string(action_count) + " actions";
  }

  // Add action types if available
  if (!unique_types.empty()) {
    summary += " (";
    for (size_t i = 0; i < unique_types.size(); i++) {
      if (i > 0)
        summary += ", ";
      summary += unique_types[i];
    }
    if (unique_types.size() < (size_t)action_count &&
        (size_t)action_count > unique_types.size()) {
      summary += "...";
    }
    summary += ")";
  }

  return summary;
}

// Legacy color namespace for compatibility
namespace Colors {
constexpr int StatusGreen = 0xFF88FF88;
constexpr int StatusRed = 0xFF8888FF;
constexpr int StatusYellow = 0xFF88FFFF;
// These will be replaced by g_theme values
constexpr int HeaderText = 0xFFE0E0E0;
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

MagdaImGuiChat::~MagdaImGuiChat() {
  // Wait for any pending async request to complete
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }
  m_ctx = nullptr;
}

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

void MagdaImGuiChat::Show() {
  m_visible = true;
  // Recreate context if it was destroyed (e.g., when window was closed)
  // This ensures the window can be reopened after being closed
  if (!m_ctx && m_available) {
    int configFlags = m_ImGui_ConfigFlags_DockingEnable();
    m_ctx = m_ImGui_CreateContext("MAGDA", &configFlags);
  }
  // Don't check API health on show - it's slow and logs too much
  SetAPIStatus("Ready", 0x88FF88FF); // Green in 0xRRGGBBAA format
}

void MagdaImGuiChat::Hide() { m_visible = false; }

void MagdaImGuiChat::Toggle() {
  m_visible = !m_visible;
  if (m_visible) {
    SetAPIStatus("Ready", 0x88FF88FF); // Green in 0xRRGGBBAA format
  } else {
    // When hiding, clean up context so it can be recreated on next show
    // This matches the behavior when window is closed via X button
    m_ctx = nullptr;
  }
}

void MagdaImGuiChat::SetInputText(const char *text) {
  if (text) {
    strncpy(m_inputBuffer, text, sizeof(m_inputBuffer) - 1);
    m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';
  }
}

void MagdaImGuiChat::ShowWithInput(const char *text) {
  Show();
  SetInputText(text);
}

void MagdaImGuiChat::CheckAPIHealth() {
  WDL_FastString error_msg;
  if (s_httpClient.CheckHealth(error_msg, 3)) {
    SetAPIStatus("Connected", 0x88FF88FF); // Green
  } else {
    SetAPIStatus("Disconnected", 0xFF6666FF); // Red
  }
}

void MagdaImGuiChat::Render() {
  if (!m_available || !m_visible) {
    return;
  }

  // Create context on first use - ReaImGui contexts stay valid as long as used
  // each frame
  if (!m_ctx) {
    int configFlags = m_ImGui_ConfigFlags_DockingEnable();
    m_ctx = m_ImGui_CreateContext("MAGDA", &configFlags);
    if (!m_ctx) {
      return;
    }
  }

  // Set initial window size
  int cond = ImGuiCond::FirstUseEver;
  m_ImGui_SetNextWindowSize(m_ctx, 800, 600, &cond);

  // Apply pending dock ID (negative = REAPER docker, 0 = floating)
  if (m_hasPendingDock) {
    m_ImGui_SetNextWindowDockID(m_ctx, m_pendingDockID, nullptr);
    m_hasPendingDock = false;
  }

  // Load color index functions from ReaImGui
  int (*Col_WindowBg)() = (int (*)())g_rec->GetFunc("ImGui_Col_WindowBg");
  int (*Col_ChildBg)() = (int (*)())g_rec->GetFunc("ImGui_Col_ChildBg");
  int (*Col_Text)() = (int (*)())g_rec->GetFunc("ImGui_Col_Text");
  int (*Col_FrameBg)() = (int (*)())g_rec->GetFunc("ImGui_Col_FrameBg");
  int (*Col_FrameBgHovered)() =
      (int (*)())g_rec->GetFunc("ImGui_Col_FrameBgHovered");
  int (*Col_FrameBgActive)() =
      (int (*)())g_rec->GetFunc("ImGui_Col_FrameBgActive");
  int (*Col_Button)() = (int (*)())g_rec->GetFunc("ImGui_Col_Button");
  int (*Col_ButtonHovered)() =
      (int (*)())g_rec->GetFunc("ImGui_Col_ButtonHovered");
  int (*Col_ButtonActive)() =
      (int (*)())g_rec->GetFunc("ImGui_Col_ButtonActive");
  int (*Col_Border)() = (int (*)())g_rec->GetFunc("ImGui_Col_Border");
  int (*Col_Separator)() = (int (*)())g_rec->GetFunc("ImGui_Col_Separator");
  int (*Col_ScrollbarBg)() = (int (*)())g_rec->GetFunc("ImGui_Col_ScrollbarBg");
  int (*Col_ScrollbarGrab)() =
      (int (*)())g_rec->GetFunc("ImGui_Col_ScrollbarGrab");

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
    m_ImGui_PushStyleColor(m_ctx, Col_Separator(), g_theme.border);
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

  bool open = true;
  int flags = ImGuiWindowFlags::NoCollapse;
  bool visible = m_ImGui_Begin(m_ctx, "MAGDA Chat", &open, &flags);

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
      // ReaImGui uses negative dock IDs for REAPER's native dockers
      if (m_ImGui_MenuItem(m_ctx, "Docker 1 (Bottom)", nullptr, nullptr,
                           nullptr)) {
        m_pendingDockID = -1;
        m_hasPendingDock = true;
      }
      if (m_ImGui_MenuItem(m_ctx, "Docker 2", nullptr, nullptr, nullptr)) {
        m_pendingDockID = -2;
        m_hasPendingDock = true;
      }
      if (m_ImGui_MenuItem(m_ctx, "Docker 3", nullptr, nullptr, nullptr)) {
        m_pendingDockID = -3;
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
    m_ImGui_TextColored(m_ctx, g_theme.headerText,
                        "MAGDA - AI Music Production Assistant");
    m_ImGui_Separator(m_ctx);

    // Input area
    m_ImGui_InputText(m_ctx, "##input", m_inputBuffer, sizeof(m_inputBuffer),
                      nullptr, nullptr);

    // Detect @ trigger for autocomplete
    DetectAtTrigger();

    // Handle keyboard for autocomplete navigation
    if (m_showAutocomplete && !m_suggestions.empty()) {
      bool repeatTrue = true;
      bool repeatFalse = false;

      // Count selectable items (exclude separators)
      int selectableCount = 0;
      for (const auto &s : m_suggestions) {
        if (s.plugin_type != "separator") selectableCount++;
      }

      if (selectableCount > 0) {
        // Up arrow - navigate up
        if (m_ImGui_IsKeyPressed &&
            m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::UpArrow, &repeatTrue)) {
          m_autocompleteIndex =
              (m_autocompleteIndex - 1 + selectableCount) % selectableCount;
        }
        // Down arrow - navigate down
        if (m_ImGui_IsKeyPressed &&
            m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::DownArrow, &repeatTrue)) {
          m_autocompleteIndex = (m_autocompleteIndex + 1) % selectableCount;
        }
        // Tab or Enter - accept completion
        if (m_ImGui_IsKeyPressed &&
            (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Tab, &repeatFalse) ||
             m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Enter, &repeatFalse))) {
          // Find the nth selectable item
          int idx = 0;
          for (const auto &s : m_suggestions) {
            if (s.plugin_type != "separator") {
              if (idx == m_autocompleteIndex) {
                InsertCompletion(s.alias);
                break;
              }
              idx++;
            }
          }
          m_showAutocomplete = false;
        }
      }
      // Escape - close autocomplete
      if (m_ImGui_IsKeyPressed &&
          m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Escape, &repeatFalse)) {
        m_showAutocomplete = false;
      }

      // Render autocomplete dropdown
      RenderAutocompletePopup();
    }

    double btnSpacing = 5;
    double zero = 0;
    m_ImGui_SameLine(m_ctx, &zero, &btnSpacing);

    // Only send on button click (Enter key handling would need separate logic)
    if (m_ImGui_Button(m_ctx, m_busy ? "..." : "Send", nullptr, nullptr)) {
      if (!m_busy && strlen(m_inputBuffer) > 0) {
        std::string msg = m_inputBuffer;
        AddUserMessage(msg);
        m_inputBuffer[0] = '\0';

        // Check for @mix: command first
        if (HandleMixCommand(msg)) {
          // Mix command handled, don't send to regular API
        } else {
          // Start async request - this won't block the UI
          StartAsyncRequest(msg);

          if (m_onSend) {
            m_onSend(msg);
          }
        }
      }
    }

    // Check for completed async request and process result
    ProcessAsyncResult();

    m_ImGui_Separator(m_ctx);

    // 3-column layout: Request | Response | Controls
    int borderFlags = 1; // ChildFlags_Borders
    int scrollFlags = ImGuiWindowFlags::AlwaysVerticalScrollbar;

    // Get available width and divide evenly
    double availW, availH;
    m_ImGui_GetContentRegionAvail(m_ctx, &availW, &availH);
    double colSpacing = 8.0;              // spacing between columns
    double totalSpacing = colSpacing * 2; // 2 gaps for 3 columns
    double colW = (availW - totalSpacing) / 3.0;

    double col1W = colW; // Request
    double col2W = colW; // Response
    double col3W = colW; // Controls
    double paneH = -30;  // Leave room for footer

    // Column 1: REQUEST (user messages)
    if (m_ImGui_BeginChild(m_ctx, "##request", &col1W, &paneH, &borderFlags,
                           &scrollFlags)) {
      m_ImGui_TextColored(m_ctx, g_theme.headerText, "REQUEST");
      m_ImGui_Separator(m_ctx);
      for (const auto &msg : m_history) {
        if (!msg.is_user)
          continue;
        RenderMessageWithHighlighting(msg.content);
        m_ImGui_Separator(m_ctx);
      }
    }
    m_ImGui_EndChild(m_ctx);

    m_ImGui_SameLine(m_ctx, &zero, &colSpacing);

    // Column 2: RESPONSE (assistant messages)
    if (m_ImGui_BeginChild(m_ctx, "##response", &col2W, &paneH, &borderFlags,
                           &scrollFlags)) {
      m_ImGui_TextColored(m_ctx, g_theme.headerText, "RESPONSE");
      m_ImGui_Separator(m_ctx);
      for (const auto &msg : m_history) {
        if (msg.is_user)
          continue;
        RenderMessageWithHighlighting(msg.content);
        m_ImGui_Separator(m_ctx);
      }
      if (!m_streamingBuffer.empty()) {
        m_ImGui_TextWrapped(m_ctx, m_streamingBuffer.c_str());
      }
      
      // Show Yes/No buttons for pending mix actions
      if (m_hasPendingMixActions) {
        m_ImGui_Separator(m_ctx);
        m_ImGui_TextColored(m_ctx, 0xFFFFAAAA, "Apply these changes?");
        m_ImGui_Dummy(m_ctx, 0, 5);
        
        double btnW = 80;
        double btnH = 0;
        
        // Green "Yes" button
        m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF338833);
        if (m_ImGui_Button(m_ctx, "Yes, Apply", &btnW, &btnH)) {
          // Execute the pending actions
          WDL_FastString execution_result, execution_error;
          if (MagdaActions::ExecuteActions(m_pendingMixActionsJson.c_str(),
                                           execution_result, execution_error)) {
            AddAssistantMessage("Changes applied successfully!");
          } else {
            std::string errMsg = "Failed to apply changes: ";
            errMsg += execution_error.Get();
            AddAssistantMessage(errMsg);
          }
          m_hasPendingMixActions = false;
          m_pendingMixActionsJson.clear();
        }
        m_ImGui_PopStyleColor(m_ctx, nullptr);
        
        m_ImGui_SameLine(m_ctx, &zero, &zero);
        
        // Red "No" button
        m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF333388);
        if (m_ImGui_Button(m_ctx, "No, Cancel", &btnW, &btnH)) {
          AddAssistantMessage("Changes cancelled.");
          m_hasPendingMixActions = false;
          m_pendingMixActionsJson.clear();
        }
        m_ImGui_PopStyleColor(m_ctx, nullptr);
        
        m_ImGui_Separator(m_ctx);
      }
      
      // Show loading spinner while busy
      if (m_busy) {
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
        double elapsed =
            ((double)clock() / CLOCKS_PER_SEC) - m_spinnerStartTime;
        int frameIndex =
            ((int)(elapsed * 10.0)) % numFrames; // 10 FPS animation

        // Build loading message with spinner
        char loadingMsg[128];
        snprintf(loadingMsg, sizeof(loadingMsg), "%s Processing request...",
                 spinnerFrames[frameIndex]);
        m_ImGui_TextColored(m_ctx, g_theme.statusYellow, loadingMsg);
        m_scrollToBottom = true; // Keep scrolling to show spinner
      }
      if (m_scrollToBottom) {
        double ratio = 1.0;
        m_ImGui_SetScrollHereY(m_ctx, &ratio);
        m_scrollToBottom = false;
      }
    }
    m_ImGui_EndChild(m_ctx);

    m_ImGui_SameLine(m_ctx, &zero, &colSpacing);

    // Column 3: CONTROLS (on right)
    if (m_ImGui_BeginChild(m_ctx, "##controls", &col3W, &paneH, &borderFlags,
                           nullptr)) {
      m_ImGui_TextColored(m_ctx, g_theme.headerText, "ACTIONS");
      m_ImGui_Separator(m_ctx);
      if (m_ImGui_Button(m_ctx, "Mix Analysis", nullptr, nullptr)) {
        // Trigger mix analysis workflow (bounce/analyze/send to agent)
        magdaAction(g_cmdMixAnalyze, 0);
      }
      if (m_ImGui_Button(m_ctx, "Master Analysis", nullptr, nullptr)) {
        if (m_onSend)
          m_onSend("Analyze the master bus and suggest mastering adjustments");
      }
      if (m_ImGui_Button(m_ctx, "Gain Staging", nullptr, nullptr)) {
        if (m_onSend)
          m_onSend("Check gain staging across all tracks");
      }
      if (m_ImGui_Button(m_ctx, "Housekeeping", nullptr, nullptr)) {
        if (m_onSend)
          m_onSend("Clean up and organize this project");
      }
    }
    m_ImGui_EndChild(m_ctx);

    // Footer with colored circle indicator
    m_ImGui_Separator(m_ctx);
    m_ImGui_TextColored(m_ctx, m_apiStatusColor,
                        "\xe2\x97\x8f"); // ● (filled circle UTF-8)
    m_ImGui_SameLine(m_ctx, &zero, &zero);
    m_ImGui_TextColored(m_ctx, g_theme.dimText, " Status: ");
    m_ImGui_SameLine(m_ctx, &zero, &zero);
    m_ImGui_TextColored(m_ctx, m_apiStatusColor, m_apiStatus.c_str());
  }

  m_ImGui_End(m_ctx);

  // Pop style colors
  m_ImGui_PopStyleColor(m_ctx, &styleColorCount);

  // Handle window close - reset context so it can be recreated next time
  if (!open) {
    m_visible = false;
    m_ctx = nullptr; // Context will be recreated on next Show()
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

      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, g_theme.userBg);
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

      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, g_theme.assistantBg);
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
      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::ChildBg, g_theme.assistantBg);
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

    // Show Yes/No buttons for pending mix actions (compact view)
    if (m_hasPendingMixActions) {
      m_ImGui_Separator(m_ctx);
      m_ImGui_TextColored(m_ctx, 0xFFFFAAAA, "Apply these changes?");
      m_ImGui_Dummy(m_ctx, 0, 5);
      
      double btnW = 80;
      double btnH = 0;
      double spacing = 10;
      
      // Green "Yes" button
      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF338833);
      if (m_ImGui_Button(m_ctx, "Yes, Apply", &btnW, &btnH)) {
        WDL_FastString execution_result, execution_error;
        if (MagdaActions::ExecuteActions(m_pendingMixActionsJson.c_str(),
                                         execution_result, execution_error)) {
          AddAssistantMessage("Changes applied successfully!");
        } else {
          std::string errMsg = "Failed to apply changes: ";
          errMsg += execution_error.Get();
          AddAssistantMessage(errMsg);
        }
        m_hasPendingMixActions = false;
        m_pendingMixActionsJson.clear();
      }
      m_ImGui_PopStyleColor(m_ctx, nullptr);
      
      m_ImGui_SameLine(m_ctx, nullptr, &spacing);
      
      // Red "No" button
      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF333388);
      if (m_ImGui_Button(m_ctx, "No, Cancel", &btnW, &btnH)) {
        AddAssistantMessage("Changes cancelled.");
        m_hasPendingMixActions = false;
        m_pendingMixActionsJson.clear();
      }
      m_ImGui_PopStyleColor(m_ctx, nullptr);
      
      m_ImGui_Separator(m_ctx);
    }

    // Show loading spinner while busy
    if (m_busy) {
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

      // Build loading message with spinner
      char loadingMsg[128];
      snprintf(loadingMsg, sizeof(loadingMsg), "%s Processing request...",
               spinnerFrames[frameIndex]);
      m_ImGui_TextColored(m_ctx, g_theme.statusYellow, loadingMsg);
      m_scrollToBottom = true; // Keep scrolling to show spinner
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
    // Trigger mix analysis workflow (bounce/analyze/send to agent)
    magdaAction(g_cmdMixAnalyze, 0);
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

  m_ImGui_Separator(m_ctx);
  m_ImGui_Dummy(m_ctx, 0, 10);

  m_ImGui_Text(m_ctx, "Chat:");
  m_ImGui_Dummy(m_ctx, 0, 5);

  if (m_ImGui_Button(m_ctx, "Clear Chat", &btnWidth, &btnHeight)) {
    m_history.clear();
    m_streamingBuffer.clear();
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  if (m_ImGui_Button(m_ctx, "Export Chat...", &btnWidth, &btnHeight)) {
    // Export chat to file
    if (g_rec) {
      char filename[1024] = {0};
      bool (*GetUserFileNameForRead)(char *, int, const char *, const char *) =
          (bool (*)(char *, int, const char *, const char *))g_rec->GetFunc(
              "GetUserFileNameForWrite");
      if (GetUserFileNameForRead &&
          GetUserFileNameForRead(filename, sizeof(filename), "",
                                 "Text Files (*.txt)\0*.txt\0")) {
        FILE *f = fopen(filename, "w");
        if (f) {
          for (const auto &msg : m_history) {
            fprintf(f, "%s: %s\n\n", msg.is_user ? "USER" : "ASSISTANT",
                    msg.content.c_str());
          }
          fclose(f);
        }
      }
    }
  }
}

void MagdaImGuiChat::RenderFooter() {
  m_ImGui_TextColored(m_ctx, g_theme.dimText, "Status: ");
  double offset = 0;
  double spacing = 0;
  m_ImGui_SameLine(m_ctx, &offset, &spacing);
  m_ImGui_TextColored(m_ctx, m_apiStatusColor, m_apiStatus.c_str());
}

void MagdaImGuiChat::RenderInputArea() {
  int flags = ImGuiInputTextFlags::EnterReturnsTrue;

  bool submitted = m_ImGui_InputText(m_ctx, "##input", m_inputBuffer,
                                     sizeof(m_inputBuffer), &flags, nullptr);

  DetectAtTrigger();

  bool repeatTrue = true;
  bool repeatFalse = false;

  if (m_showAutocomplete && !m_suggestions.empty()) {
    // Count selectable items (exclude separators)
    int selectableCount = 0;
    for (const auto &s : m_suggestions) {
      if (s.plugin_type != "separator") selectableCount++;
    }

    if (selectableCount > 0) {
      // Autocomplete navigation
      if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::UpArrow, &repeatTrue)) {
        m_autocompleteIndex =
            (m_autocompleteIndex - 1 + selectableCount) % selectableCount;
      }
      if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::DownArrow, &repeatTrue)) {
        m_autocompleteIndex = (m_autocompleteIndex + 1) % selectableCount;
      }
      if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Tab, &repeatFalse) ||
          m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Enter, &repeatFalse)) {
        // Find the nth selectable item
        int idx = 0;
        for (const auto &s : m_suggestions) {
          if (s.plugin_type != "separator") {
            if (idx == m_autocompleteIndex) {
              InsertCompletion(s.alias);
              break;
            }
            idx++;
          }
        }
        m_showAutocomplete = false;
        return;
      }
    }
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::Escape, &repeatFalse)) {
      m_showAutocomplete = false;
    }
  } else if (!m_inputHistory.empty()) {
    // Command history navigation (when autocomplete is not active)
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::UpArrow, &repeatFalse)) {
      if (m_inputHistoryIndex == -1) {
        // Save current input before navigating
        m_savedInput = m_inputBuffer;
        m_inputHistoryIndex = (int)m_inputHistory.size() - 1;
      } else if (m_inputHistoryIndex > 0) {
        m_inputHistoryIndex--;
      }
      if (m_inputHistoryIndex >= 0 &&
          m_inputHistoryIndex < (int)m_inputHistory.size()) {
        strncpy(m_inputBuffer, m_inputHistory[m_inputHistoryIndex].c_str(),
                sizeof(m_inputBuffer) - 1);
        m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';
      }
    }
    if (m_ImGui_IsKeyPressed(m_ctx, ImGuiKey::DownArrow, &repeatFalse)) {
      if (m_inputHistoryIndex >= 0) {
        m_inputHistoryIndex++;
        if (m_inputHistoryIndex >= (int)m_inputHistory.size()) {
          // Restore saved input
          m_inputHistoryIndex = -1;
          strncpy(m_inputBuffer, m_savedInput.c_str(),
                  sizeof(m_inputBuffer) - 1);
          m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';
        } else {
          strncpy(m_inputBuffer, m_inputHistory[m_inputHistoryIndex].c_str(),
                  sizeof(m_inputBuffer) - 1);
          m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';
        }
      }
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
    if (canSend) {
      std::string msg = m_inputBuffer;
      // Add to command history
      m_inputHistory.push_back(msg);
      m_inputHistoryIndex = -1;
      m_savedInput.clear();

      AddUserMessage(msg);
      m_inputBuffer[0] = '\0';
      m_showAutocomplete = false;

      // Check for @mix: command first
      if (HandleMixCommand(msg)) {
        // Mix command handled, don't send to regular API
      } else if (m_onSend) {
        m_onSend(msg);
      }
    }
  }

  if (!canSend) {
    int popCount = 1;
    m_ImGui_PopStyleColor(m_ctx, &popCount);
  }
}

void MagdaImGuiChat::RenderAutocompletePopup() {
  // Draw as a child window - doesn't steal focus like popup
  double acWidth = 400;
  double acHeight = 200;
  int childFlags = 1; // Border
  int windowFlags = 0;

  // Push darker background for autocomplete dropdown
  m_ImGui_PushStyleColor(m_ctx, 7, g_theme.childBg); // Col_ChildBg

  // Take a copy of suggestions to avoid issues during iteration
  std::vector<AutocompleteSuggestion> localSuggestions = m_suggestions;
  std::string selectedAlias;
  bool wasSelected = false;

  if (m_ImGui_BeginChild(m_ctx, "##autocomplete_list", &acWidth, &acHeight,
                         &childFlags, &windowFlags)) {
    int idx = 0;
    int selectableIdx = 0; // Index excluding separators
    int maxItems = 20; // Show more items
    for (const auto &suggestion : localSuggestions) {
      // Handle separator
      if (suggestion.plugin_type == "separator") {
        m_ImGui_Separator(m_ctx);
        m_ImGui_TextColored(m_ctx, g_theme.dimText, "── Plugins ──");
        m_ImGui_Separator(m_ctx);
        idx++;
        continue;
      }

      bool wasHighlighted = (selectableIdx == m_autocompleteIndex);
      bool isSelected = wasHighlighted;

      // Highlight selected item
      if (wasHighlighted) {
        m_ImGui_PushStyleColor(m_ctx, 24, g_theme.buttonBg); // Col_Header
      }

      std::string label =
          "@" + suggestion.alias + " - " + suggestion.plugin_name;

      if (m_ImGui_Selectable(m_ctx, label.c_str(), &isSelected, nullptr,
                             nullptr, nullptr)) {
        selectedAlias = suggestion.alias;
        wasSelected = true;
      }

      if (wasHighlighted) {
        m_ImGui_PopStyleColor(m_ctx, nullptr);
      }

      idx++;
      selectableIdx++;
      if (idx >= maxItems)
        break;
    }
  }
  m_ImGui_EndChild(m_ctx);
  m_ImGui_PopStyleColor(m_ctx, nullptr);

  // Handle selection after loop to avoid modifying state during iteration
  if (wasSelected) {
    InsertCompletion(selectedAlias);
    m_showAutocomplete = false;
  }
}

void MagdaImGuiChat::RenderMessageWithHighlighting(const std::string &content) {
  // Parse and render with @mention highlighting
  size_t pos = 0;
  size_t len = content.length();
  int mentionColor = THEME_RGBA(0x66, 0xCC, 0xFF); // Cyan for @mentions

  while (pos < len) {
    size_t atPos = content.find('@', pos);

    if (atPos == std::string::npos) {
      // No more @, render rest as normal
      if (pos < len) {
        m_ImGui_Text(m_ctx, content.substr(pos).c_str());
      }
      break;
    }

    // Render text before @
    if (atPos > pos) {
      std::string before = content.substr(pos, atPos - pos);
      m_ImGui_Text(m_ctx, before.c_str());
      m_ImGui_SameLine(m_ctx, nullptr, nullptr);
    }

    // Find end of @mention (next space or end)
    size_t endPos = content.find(' ', atPos);
    if (endPos == std::string::npos) {
      endPos = len;
    }

    // Render @mention in cyan
    std::string mention = content.substr(atPos, endPos - atPos);
    m_ImGui_TextColored(m_ctx, mentionColor, mention.c_str());

    if (endPos < len) {
      m_ImGui_SameLine(m_ctx, nullptr, nullptr);
    }

    pos = endPos;
  }
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
  
  // Count selectable items (exclude separators)
  int selectableCount = 0;
  for (const auto &s : m_suggestions) {
    if (s.plugin_type != "separator") selectableCount++;
  }
  
  m_showAutocomplete = selectableCount > 0;
  m_autocompleteIndex = 0;
}

void MagdaImGuiChat::UpdateAutocompleteSuggestions() {
  m_suggestions.clear();

  std::string query = m_autocompletePrefix;
  std::transform(query.begin(), query.end(), query.begin(), ::tolower);

  // Add mix analysis types
  static const std::vector<std::pair<std::string, std::string>> mixTypes = {
      {"mix:drums", "Analyze drums/percussion track"},
      {"mix:bass", "Analyze bass track"},
      {"mix:synth", "Analyze synth/pad track"},
      {"mix:vocals", "Analyze vocal track"},
      {"mix:guitar", "Analyze guitar track"},
      {"mix:piano", "Analyze piano/keys track"},
      {"mix:strings", "Analyze strings track"},
      {"mix:fx", "Analyze FX/sound design track"},
  };

  for (const auto &pair : mixTypes) {
    const std::string &alias = pair.first;
    const std::string &desc = pair.second;

    std::string aliasLower = alias;
    std::transform(aliasLower.begin(), aliasLower.end(), aliasLower.begin(),
                   ::tolower);

    if (query.empty() || aliasLower.find(query) == 0) {
      AutocompleteSuggestion suggestion;
      suggestion.alias = alias;
      suggestion.plugin_name = desc;
      suggestion.plugin_type = "mix";
      m_suggestions.push_back(suggestion);
    }
  }

  // Add plugin aliases with "plugin:" prefix
  if (m_pluginScanner) {
    const auto &aliases = m_pluginScanner->GetAliases();

    for (const auto &pair : aliases) {
      const std::string &alias = pair.first;
      const std::string &pluginName = pair.second;

      // Create prefixed alias for matching
      std::string prefixedAlias = "plugin:" + alias;
      std::string prefixedLower = prefixedAlias;
      std::transform(prefixedLower.begin(), prefixedLower.end(),
                     prefixedLower.begin(), ::tolower);

      if (query.empty() || prefixedLower.find(query) == 0) {
        AutocompleteSuggestion suggestion;
        suggestion.alias = prefixedAlias;
        suggestion.plugin_name = pluginName;
        suggestion.plugin_type = "plugin";
        m_suggestions.push_back(suggestion);
      }
    }
  }

  std::sort(m_suggestions.begin(), m_suggestions.end(),
            [&query](const AutocompleteSuggestion &a,
                     const AutocompleteSuggestion &b) {
              // Mix types first, then plugins
              if (a.plugin_type != b.plugin_type) {
                return a.plugin_type == "mix";
              }
              bool aStartsWith = a.alias.find(query) == 0;
              bool bStartsWith = b.alias.find(query) == 0;
              if (aStartsWith != bStartsWith) {
                return aStartsWith;
              }
              return a.alias < b.alias;
            });

  // Insert separator between mix and plugin types
  bool foundMix = false;
  bool insertedSeparator = false;
  for (size_t i = 0; i < m_suggestions.size() && !insertedSeparator; i++) {
    if (m_suggestions[i].plugin_type == "mix") {
      foundMix = true;
    } else if (foundMix && m_suggestions[i].plugin_type == "plugin") {
      // Insert separator before first plugin
      AutocompleteSuggestion separator;
      separator.alias = "---";
      separator.plugin_name = "";
      separator.plugin_type = "separator";
      m_suggestions.insert(m_suggestions.begin() + i, separator);
      insertedSeparator = true;
    }
  }
}

void MagdaImGuiChat::InsertCompletion(const std::string &alias) {
  if (m_atPosition == std::string::npos) {
    return;
  }

  std::string input = m_inputBuffer;
  std::string before = input.substr(0, m_atPosition);
  std::string completion = "@" + alias + " ";

  // Find end of current @mention (next space or end of string)
  size_t afterAt = m_atPosition + 1 + m_autocompletePrefix.length();
  std::string after = (afterAt < input.length()) ? input.substr(afterAt) : "";

  std::string newInput = before + completion + after;

  strncpy(m_inputBuffer, newInput.c_str(), sizeof(m_inputBuffer) - 1);
  m_inputBuffer[sizeof(m_inputBuffer) - 1] = '\0';

  m_atPosition = std::string::npos;
}

// Check if message is a @mix: command and handle it
// Returns true if handled (should not be sent to regular API)
bool MagdaImGuiChat::HandleMixCommand(const std::string &msg) {
  // Check for @mix: prefix
  size_t mixPos = msg.find("@mix:");
  if (mixPos == std::string::npos) {
    return false;
  }

  // Extract track type and query
  std::string afterMix = msg.substr(mixPos + 5); // After "@mix:"

  // Find the track type (next word)
  size_t typeStart = afterMix.find_first_not_of(" ");
  if (typeStart == std::string::npos) {
    AddAssistantMessage("Error: Please specify a track type after @mix: (e.g., @mix:synth make it brighter)");
    return true;
  }

  size_t typeEnd = afterMix.find(' ', typeStart);
  std::string trackType;
  std::string userQuery;

  if (typeEnd == std::string::npos) {
    trackType = afterMix.substr(typeStart);
    userQuery = "";
  } else {
    trackType = afterMix.substr(typeStart, typeEnd - typeStart);
    userQuery = afterMix.substr(typeEnd + 1);
    // Trim leading spaces from query
    size_t queryStart = userQuery.find_first_not_of(" ");
    if (queryStart != std::string::npos) {
      userQuery = userQuery.substr(queryStart);
    }
  }

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char logMsg[512];
    snprintf(logMsg, sizeof(logMsg),
             "MAGDA: Mix analysis - type: '%s', query: '%s'\n",
             trackType.c_str(), userQuery.c_str());
    ShowConsoleMsg(logMsg);
  }

  // Clear any pending result from previous run
  MagdaBounceWorkflow::ClearPendingResult();

  // Execute the mix analysis workflow
  WDL_FastString error_msg;
  bool success = MagdaBounceWorkflow::ExecuteWorkflow(
      BOUNCE_MODE_FULL_TRACK, trackType.c_str(), userQuery.c_str(), error_msg);

  if (!success) {
    std::string errorStr = "Mix analysis failed: ";
    errorStr += error_msg.Get();
    AddAssistantMessage(errorStr);
  } else {
    // Set busy state to show spinner
    m_busy = true;
    m_spinnerStartTime = (double)clock() / CLOCKS_PER_SEC;
    SetAPIStatus("Analyzing...", 0xFFFF66FF); // Yellow
  }

  return true;
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

void MagdaImGuiChat::StartAsyncRequest(const std::string &question) {
  // Don't start a new request if one is already pending
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    if (m_asyncPending) {
      return;
    }
  }

  // Set busy state and start spinner animation
  m_busy = true;
  m_spinnerStartTime = (double)clock() / CLOCKS_PER_SEC;
  SetAPIStatus("Sending...", 0xFFFF66FF); // Yellow

  // Set backend URL from settings
  const char *backendUrl = MagdaSettingsWindow::GetBackendURL();
  if (backendUrl && backendUrl[0]) {
    s_httpClient.SetBackendURL(backendUrl);
  }

  // Set JWT token if available
  const char *token = MagdaLoginWindow::GetStoredToken();
  if (token && token[0]) {
    s_httpClient.SetJWTToken(token);
  }

  // Build request JSON on main thread (accesses REAPER state)
  WDL_FastString request_json;
  request_json.Append("{\"question\":\"");
  // Escape question string
  for (const char *p = question.c_str(); *p; p++) {
    switch (*p) {
    case '"':
      request_json.Append("\\\"");
      break;
    case '\\':
      request_json.Append("\\\\");
      break;
    case '\n':
      request_json.Append("\\n");
      break;
    case '\r':
      request_json.Append("\\r");
      break;
    case '\t':
      request_json.Append("\\t");
      break;
    default:
      request_json.Append(p, 1);
      break;
    }
  }
  request_json.Append("\",\"state\":");

  // Get REAPER state on main thread (before spawning async)
  char *state_json = MagdaState::GetStateSnapshot();
  if (state_json) {
    request_json.Append(state_json);
    free(state_json);
  } else {
    request_json.Append("{}");
  }
  request_json.Append("}");

  // Store the request JSON and mark as pending
  std::string requestJsonStr = request_json.Get();
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    m_pendingQuestion = question;
    m_asyncPending = true;
    m_asyncResultReady = false;
    m_asyncSuccess = false;
    m_asyncResponseJson.clear();
    m_asyncErrorMsg.clear();
  }

  // Wait for any previous thread to finish
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }

  // Start the async request thread - ONLY do HTTP, no REAPER API calls
  m_asyncThread = std::thread([this, requestJsonStr]() {
    // Make the HTTP request (this is the slow part)
    // Use SendPOSTRequest directly - do NOT call SendQuestion which executes
    // actions
    WDL_FastString response, error_msg;
    bool success = s_httpClient.SendPOSTRequest(
        "/api/v1/chat", requestJsonStr.c_str(), response, error_msg, 60);

    // Store the result
    {
      std::lock_guard<std::mutex> lock(m_asyncMutex);
      m_asyncSuccess = success;
      if (success) {
        m_asyncResponseJson = response.Get();
      } else {
        m_asyncErrorMsg = error_msg.Get();
      }
      m_asyncResultReady = true;
      m_asyncPending = false;
    }
  });
}

void MagdaImGuiChat::ProcessAsyncResult() {
  // First check for mix analysis results
  {
    MixAnalysisResult mixResult;
    if (MagdaBounceWorkflow::GetPendingResult(mixResult)) {
      MagdaBounceWorkflow::ClearPendingResult();
      
      if (mixResult.success) {
        // Add the response text
        AddAssistantMessage(mixResult.responseText);
        
        // Store pending actions if any
        if (!mixResult.actionsJson.empty() && mixResult.actionsJson != "[]") {
          m_hasPendingMixActions = true;
          m_pendingMixActionsJson = mixResult.actionsJson;
        }
        
        SetAPIStatus("Connected", 0x88FF88FF); // Green
      } else {
        std::string errorStr = "Mix analysis error: ";
        errorStr += mixResult.responseText;
        AddAssistantMessage(errorStr);
        SetAPIStatus("Error", 0xFF6666FF); // Red
      }
      m_busy = false;
      return;
    }
  }

  bool resultReady = false;
  bool success = false;
  std::string responseJson;
  std::string errorMsg;

  // Check if result is ready (thread-safe)
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    if (!m_asyncResultReady) {
      return;
    }
    resultReady = true;
    success = m_asyncSuccess;
    responseJson = m_asyncResponseJson;
    errorMsg = m_asyncErrorMsg;
    m_asyncResultReady = false; // Mark as processed
  }

  if (!resultReady) {
    return;
  }

  // Join the thread if it's done
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }

  // Process the result on the MAIN thread (safe to call REAPER APIs here)
  if (success) {
    // Execute actions from the response - MUST be on main thread
    if (!responseJson.empty()) {
      // Extract actions JSON from response
      char *actions_json = MagdaHTTPClient::ExtractActionsJSON(
          responseJson.c_str(), (int)responseJson.length());

      if (actions_json) {
        WDL_FastString execution_result, execution_error;
        if (!MagdaActions::ExecuteActions(actions_json, execution_result,
                                          execution_error)) {
          // Log error
          if (g_rec) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              char log_msg[512];
              snprintf(log_msg, sizeof(log_msg),
                       "MAGDA: Action execution failed: %s\n",
                       execution_error.Get());
              ShowConsoleMsg(log_msg);
            }
          }
        }
        free(actions_json);
      }
    }

    // Extract summary of what was done
    std::string summary = ExtractActionSummary(responseJson.c_str());
    AddAssistantMessage(summary);
    SetAPIStatus("Connected", 0x88FF88FF); // Green
  } else {
    // Error - show more context
    std::string errorStr = "Error: ";
    errorStr += errorMsg;
    AddAssistantMessage(errorStr);
    SetAPIStatus("Error", 0xFF6666FF); // Red
  }

  m_busy = false;
}
