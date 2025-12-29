#include "magda_imgui_chat.h"
#include "../WDL/WDL/jsonparse.h"
#include "../api/magda_agents.h"
#include "../api/magda_openai.h"
#include "../dsl/magda_actions.h"
#include "../dsl/magda_arranger_interpreter.h"
#include "../dsl/magda_drummer_interpreter.h"
#include "../dsl/magda_dsl_context.h"
#include "../dsl/magda_dsl_grammar.h"
#include "../dsl/magda_dsl_interpreter.h"
#include "../dsl/magda_jsfx_interpreter.h"
#include "magda_actions.h"
#include "magda_api_client.h"
#include "magda_bounce_workflow.h"
#include "magda_imgui_api_keys.h"
#include "magda_imgui_login.h"
#include "magda_imgui_settings.h"
#include "magda_plugin_scanner.h"
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
extern int g_cmdMasterAnalyze;

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

// Helper function to format a single action as readable text
// Format action as concise technical output for streaming display
static std::string FormatAction(wdl_json_element *action, int index) {
  if (!action)
    return "";

  std::string result;
  char buf[512];

  // Get action type
  wdl_json_element *action_type = action->get_item_by_name("action");
  if (!action_type || !action_type->m_value_string) {
    return "";
  }

  const char *type = action_type->m_value;

  // Format: [N] action_type: key=value, key=value
  snprintf(buf, sizeof(buf), "[%d] %s:", index + 1, type);
  result = buf;

  // Add key parameters based on what's present
  bool first = true;

  // Common parameters to show
  const char *params[] = {"track",       "name",       "index", "bar",
                          "length_bars", "instrument", "fx",    "position",
                          "color",       "selected",   nullptr};

  for (int i = 0; params[i]; i++) {
    const char *val = action->get_string_by_name(params[i], true);
    if (val && val[0]) {
      result += first ? " " : ", ";
      result += params[i];
      result += "=";
      result += val;
      first = false;
    }
  }

  // Check for notes array (MIDI)
  wdl_json_element *notes_elem = action->get_item_by_name("notes");
  if (notes_elem && notes_elem->is_array()) {
    int note_count = 0;
    int idx = 0;
    while (notes_elem->enum_item(idx)) {
      note_count++;
      idx++;
    }
    if (note_count > 0) {
      snprintf(buf, sizeof(buf), "%s notes=%d", first ? " " : ", ", note_count);
      result += buf;
    }
  }

  return result;
}

// Helper function to extract and format all actions from response JSON
static std::string FormatAllActions(const char *response_json) {
  if (!response_json || !response_json[0]) {
    return "Done (no actions)";
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

  std::string result;
  int action_count = 0;

  // Iterate through array using enum_item
  int idx = 0;
  wdl_json_element *action = actions->enum_item(idx);
  while (action) {
    std::string formatted = FormatAction(action, action_count);
    if (!formatted.empty()) {
      if (!result.empty()) {
        result += "\n";
      }
      result += formatted;
      action_count++;
    }
    idx++;
    action = actions->enum_item(idx);
  }

  if (action_count == 0) {
    return "Done (no actions)";
  }

  return result;
}

// Helper function to extract action summary from response JSON
static std::string ExtractActionSummary(const char *response_json) {
  // Use FormatAllActions to show all actions
  return FormatAllActions(response_json);
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
        if (s.plugin_type != "separator")
          selectableCount++;
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

    // Hide Send/Cancel button when autocomplete is showing to prevent accidental clicks
    if (!m_showAutocomplete) {
      if (m_busy) {
        // Red Cancel button when busy
        m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF4444AA);
        if (m_ImGui_Button(m_ctx, "Cancel", nullptr, nullptr)) {
          // Set cancel flag and clear busy state
          {
            std::lock_guard<std::mutex> lock(m_asyncMutex);
            m_cancelRequested = true;
            m_asyncPending = false;
            m_asyncResultReady = false;
          }
          m_busy = false;
          AddAssistantMessage("Request cancelled.");
          SetAPIStatus("Cancelled", 0xFFAAAAFF);
        }
        int popCount = 1;
        m_ImGui_PopStyleColor(m_ctx, &popCount);
      } else {
        // Normal Send button
        bool canSend = strlen(m_inputBuffer) > 0;
        if (!canSend) {
          m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF555555);
        }
        if (m_ImGui_Button(m_ctx, "Send", nullptr, nullptr)) {
          if (canSend) {
            std::string msg = m_inputBuffer;
            m_lastRequest = msg; // Store for repeat functionality
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
        if (!canSend) {
          int popCount = 1;
          m_ImGui_PopStyleColor(m_ctx, &popCount);
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
    double totalW = availW - totalSpacing;

    double col1W = totalW * 0.25; // Request (narrower)
    double col2W = totalW * 0.50; // Response (wider)
    double col3W = totalW * 0.25; // Controls (narrower)
    double paneH = -30;           // Leave room for footer

    // Single-column chat area
    double chatW = col1W + colSpacing + col2W;
    int userColor = g_theme.accent;       // Blue for user
    int assistantColor = g_theme.normalText; // Normal for assistant

    if (m_ImGui_BeginChild(m_ctx, "##chat_scroll", &chatW, &paneH, &borderFlags,
                           &scrollFlags)) {
      // Render all messages in chronological order
      for (const auto &msg : m_history) {
        if (msg.is_user) {
          // User message: show with ">" prefix in accent color
          m_ImGui_TextColored(m_ctx, userColor, "> ");
          m_ImGui_SameLine(m_ctx, nullptr, nullptr);
          m_ImGui_TextColored(m_ctx, userColor, msg.content.c_str());
        } else {
          // Assistant message: normal text
          m_ImGui_TextWrapped(m_ctx, msg.content.c_str());
        }
        m_ImGui_Separator(m_ctx);
      }

      // Update streaming text
      if (m_isStreamingText && m_streamingCharIndex < m_streamingFullText.length()) {
        double now = (double)clock() / CLOCKS_PER_SEC;
        while (m_streamingCharIndex < m_streamingFullText.length() &&
               (now - m_lastStreamCharTime) > 0.016) {
          m_streamingBuffer += m_streamingFullText[m_streamingCharIndex];
          m_streamingCharIndex++;
          m_lastStreamCharTime = now;
          m_scrollToBottom = true;
        }
        if (m_streamingCharIndex >= m_streamingFullText.length()) {
          AddAssistantMessage(m_streamingFullText);
          m_streamingBuffer.clear();
          m_streamingFullText.clear();
          m_isStreamingText = false;
        }
      }

      // Show streaming buffer or spinner
      if (!m_streamingBuffer.empty()) {
        m_ImGui_TextWrapped(m_ctx, m_streamingBuffer.c_str());
      } else if (m_busy && !m_isMixAnalysisStreaming) {
        const char *spinnerFrames[] = {
            "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
            "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
            "\xe2\xa0\x87", "\xe2\xa0\x8f"
        };
        double elapsed = ((double)clock() / CLOCKS_PER_SEC) - m_spinnerStartTime;
        int frameIndex = ((int)(elapsed * 10.0)) % 10;

        // Only show phase-specific messages for mix analysis (when phase != IDLE)
        const char *phaseMsg = "Processing request...";
        MixAnalysisPhase phase = MagdaBounceWorkflow::GetCurrentPhase();
        if (phase != MIX_PHASE_IDLE) {
          switch (phase) {
            case MIX_PHASE_RENDERING: phaseMsg = "Rendering audio..."; break;
            case MIX_PHASE_DSP_ANALYSIS: phaseMsg = "Running DSP analysis..."; break;
            case MIX_PHASE_API_CALL: phaseMsg = "Analyzing with AI..."; break;
            default: break;
          }
        }
        char loadingMsg[128];
        snprintf(loadingMsg, sizeof(loadingMsg), "%s %s", spinnerFrames[frameIndex], phaseMsg);
        m_ImGui_TextColored(m_ctx, g_theme.statusYellow, loadingMsg);
        m_scrollToBottom = true;
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
        // Trigger master analysis workflow (bounce master/analyze/send to
        // agent)
        magdaAction(g_cmdMasterAnalyze, 0);
      }

      m_ImGui_Separator(m_ctx);

      // Repeat last request button
      bool canRepeat = !m_busy && !m_lastRequest.empty();
      if (!canRepeat) {
        m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF555555);
      }
      if (m_ImGui_Button(m_ctx, "Repeat Last", nullptr, nullptr)) {
        RepeatLast();
      }
      if (!canRepeat) {
        int popCount = 1;
        m_ImGui_PopStyleColor(m_ctx, &popCount);
      }

      if (m_ImGui_Button(m_ctx, "Clear Chat", nullptr, nullptr)) {
        ClearHistory();
      }
      if (m_ImGui_Button(m_ctx, "Copy Chat", nullptr, nullptr)) {
        CopyToClipboard();
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
    double col1Weight = 0.5; // Request (narrower)
    double col2Weight = 1.0; // Response (wider)
    double col3Weight = 0.5; // Controls (narrower)
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

    // Update streaming text (typewriter effect for mix analysis responses)
    if (m_isStreamingText && m_streamingCharIndex < m_streamingFullText.length()) {
      double now = (double)clock() / CLOCKS_PER_SEC;
      while (m_streamingCharIndex < m_streamingFullText.length() &&
             (now - m_lastStreamCharTime) > 0.016) {
        m_streamingBuffer += m_streamingFullText[m_streamingCharIndex];
        m_streamingCharIndex++;
        m_lastStreamCharTime = now;
        m_scrollToBottom = true;
      }
      if (m_streamingCharIndex >= m_streamingFullText.length()) {
        AddAssistantMessage(m_streamingFullText);
        m_streamingBuffer.clear();
        m_streamingFullText.clear();
        m_isStreamingText = false;
      }
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

    // TODO: "Apply Changes" UI disabled for now - needs refinement (compact view)
    // if (m_hasPendingMixActions) { ... }

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

      // Build loading message with spinner - show phase-specific message
      // Show spinner if NOT streaming, OR if streaming but buffer is still empty
      // (buffer empty means we're waiting for the synchronous HTTP request to complete)
      if (!m_isMixAnalysisStreaming || m_lastMixStreamBuffer.empty()) {
        const char *phaseMsg = "Processing request...";
        MixAnalysisPhase phase = MagdaBounceWorkflow::GetCurrentPhase();
        switch (phase) {
          case MIX_PHASE_RENDERING:
            phaseMsg = "Rendering audio...";
            break;
          case MIX_PHASE_DSP_ANALYSIS:
            phaseMsg = "Running DSP analysis...";
            break;
          case MIX_PHASE_API_CALL:
            phaseMsg = "Analyzing with AI...";
            break;
          default:
            phaseMsg = "Processing request...";
            break;
        }
        char loadingMsg[128];
        snprintf(loadingMsg, sizeof(loadingMsg), "%s %s",
                 spinnerFrames[frameIndex], phaseMsg);
        m_ImGui_TextColored(m_ctx, g_theme.statusYellow, loadingMsg);
      }
      m_scrollToBottom = true; // Keep scrolling to show new content
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
    // Trigger master analysis workflow (bounce master/analyze/send to agent)
    magdaAction(g_cmdMasterAnalyze, 0);
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  // Repeat last request button
  bool canRepeat = !m_busy && !m_lastRequest.empty();
  if (!canRepeat) {
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF555555);
  }
  if (m_ImGui_Button(m_ctx, "Repeat Last", &btnWidth, &btnHeight)) {
    RepeatLast();
  }
  if (!canRepeat) {
    int popCount = 1;
    m_ImGui_PopStyleColor(m_ctx, &popCount);
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
    ClearHistory();
  }

  m_ImGui_Dummy(m_ctx, 0, 3);

  // Copy chat to clipboard button
  if (m_ImGui_Button(m_ctx, "Copy Chat", &btnWidth, &btnHeight)) {
    CopyToClipboard();
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
      if (s.plugin_type != "separator")
        selectableCount++;
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

  // Show Cancel button when busy, Send button otherwise
  if (m_busy) {
    // Red Cancel button
    m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF4444AA);
    if (m_ImGui_Button(m_ctx, "Cancel", nullptr, nullptr)) {
      // Set cancel flag and clear busy state
      {
        std::lock_guard<std::mutex> lock(m_asyncMutex);
        m_cancelRequested = true;
        m_asyncPending = false;
        m_asyncResultReady = false;
      }
      m_busy = false;
      AddAssistantMessage("Request cancelled.");
      SetAPIStatus("Cancelled", 0xFFAAAAFF); // Light red
    }
    int popCount = 1;
    m_ImGui_PopStyleColor(m_ctx, &popCount);
  } else {
    bool canSend = strlen(m_inputBuffer) > 0;
    if (!canSend) {
      m_ImGui_PushStyleColor(m_ctx, ImGuiCol::Button, 0xFF555555);
    }

    if (m_ImGui_Button(m_ctx, "Send", nullptr, nullptr) || submitted) {
      if (canSend) {
        std::string msg = m_inputBuffer;
        m_lastRequest = msg; // Store for repeat functionality
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
    int selectableIdx = 0; // Index excluding separators
    for (const auto &suggestion : localSuggestions) {
      // Handle separator
      if (suggestion.plugin_type == "separator") {
        m_ImGui_Separator(m_ctx);
        m_ImGui_TextColored(m_ctx, g_theme.dimText, "── Plugins ──");
        m_ImGui_Separator(m_ctx);
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

      selectableIdx++;
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
  // For mix analysis responses with multiple lines, just use TextWrapped
  // The @ highlighting is mainly for user input anyway
  if (content.find('\n') != std::string::npos) {
    // Multi-line content - use wrapped text for proper display
    m_ImGui_TextWrapped(m_ctx, content.c_str());
    return;
  }

  // Single line - parse and render with @mention highlighting
  size_t pos = 0;
  size_t len = content.length();
  int mentionColor = THEME_RGBA(0x66, 0xCC, 0xFF); // Cyan for @mentions

  while (pos < len) {
    size_t atPos = content.find('@', pos);

    if (atPos == std::string::npos) {
      // No more @, render rest as normal wrapped
      if (pos < len) {
        m_ImGui_TextWrapped(m_ctx, content.substr(pos).c_str());
      }
      break;
    }

    // Render text before @
    if (atPos > pos) {
      std::string before = content.substr(pos, atPos - pos);
      m_ImGui_TextWrapped(m_ctx, before.c_str());
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
    if (s.plugin_type != "separator")
      selectableCount++;
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

// Public action methods (callable from REAPER action system)
void MagdaImGuiChat::ClearHistory() {
  m_history.clear();
  m_streamingBuffer.clear();
}

bool MagdaImGuiChat::RepeatLast() {
  if (m_busy || m_lastRequest.empty()) {
    return false;
  }

  AddUserMessage(m_lastRequest);
  if (HandleMixCommand(m_lastRequest)) {
    // Mix command handled
  } else {
    StartAsyncRequest(m_lastRequest);
    if (m_onSend) {
      m_onSend(m_lastRequest);
    }
  }
  return true;
}

void MagdaImGuiChat::CopyToClipboard() {
  // Build chat text
  std::string chatText;
  for (const auto &msg : m_history) {
    if (msg.is_user) {
      chatText += "User: ";
    } else {
      chatText += "Assistant: ";
    }
    chatText += msg.content + "\n\n";
  }
  // Copy to clipboard using REAPER API
  if (g_rec && !chatText.empty()) {
    void (*CF_SetClipboard)(const char *) =
        (void (*)(const char *))g_rec->GetFunc("CF_SetClipboard");
    if (CF_SetClipboard) {
      CF_SetClipboard(chatText.c_str());
    }
  }
}

// Check if message is a @mix: or @master: command and handle it
// Returns true if handled (should not be sent to regular API)
bool MagdaImGuiChat::HandleMixCommand(const std::string &msg) {
  // Check for @master: prefix first (master bus analysis)
  size_t masterPos = msg.find("@master:");
  if (masterPos != std::string::npos) {
    // Extract query after @master:
    std::string afterMaster = msg.substr(masterPos + 8); // After "@master:"

    // Trim leading spaces from query
    size_t queryStart = afterMaster.find_first_not_of(" ");
    std::string userQuery;
    if (queryStart != std::string::npos) {
      userQuery = afterMaster.substr(queryStart);
    }

    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char logMsg[512];
      snprintf(logMsg, sizeof(logMsg), "MAGDA: Master analysis - query: '%s'\n",
               userQuery.c_str());
      ShowConsoleMsg(logMsg);
    }

    // Clear any pending result from previous run
    MagdaBounceWorkflow::ClearPendingResult();

    // Execute the master analysis workflow
    WDL_FastString error_msg;
    bool success = MagdaBounceWorkflow::ExecuteMasterWorkflow(userQuery.c_str(),
                                                              error_msg);

    if (!success) {
      std::string errorStr = "Master analysis failed: ";
      errorStr += error_msg.Get();
      AddAssistantMessage(errorStr);
    } else {
      // Set busy state to show spinner
      m_busy = true;
      m_spinnerStartTime = (double)clock() / CLOCKS_PER_SEC;
      SetAPIStatus("Analyzing master...", 0xFFFF66FF); // Yellow
    }

    return true;
  }

  // Check for @mix: prefix
  size_t mixPos = msg.find("@mix:");
  if (mixPos == std::string::npos) {
    return false;
  }

  // Extract command after @mix:
  std::string afterMix = msg.substr(mixPos + 5); // After "@mix:"

  // Trim leading spaces
  size_t cmdStart = afterMix.find_first_not_of(" ");
  if (cmdStart == std::string::npos) {
    AddAssistantMessage(
        "Error: Please specify a track type or comparison after @mix: (e.g., "
        "@mix:synth make it brighter or @mix:compare drums bass)");
    return true;
  }
  afterMix = afterMix.substr(cmdStart);

  // Check for "compare" keyword for multi-track comparison
  std::string lowerCmd = afterMix;
  std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

  if (lowerCmd.compare(0, 8, "compare ") == 0) {
    // Multi-track comparison mode
    std::string compareArgs = afterMix.substr(8); // After "compare "
    size_t argsStart = compareArgs.find_first_not_of(" ");
    if (argsStart != std::string::npos) {
      compareArgs = compareArgs.substr(argsStart);
    }

    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char logMsg[512];
      snprintf(logMsg, sizeof(logMsg),
               "MAGDA: Multi-track comparison - args: '%s'\n",
               compareArgs.c_str());
      ShowConsoleMsg(logMsg);
    }

    // Clear any pending result from previous run
    MagdaBounceWorkflow::ClearPendingResult();

    // Execute the multi-track comparison workflow
    WDL_FastString error_msg;
    bool success = MagdaBounceWorkflow::ExecuteMultiTrackWorkflow(
        compareArgs.c_str(), error_msg);

    if (!success) {
      std::string errorStr = "Multi-track comparison failed: ";
      errorStr += error_msg.Get();
      AddAssistantMessage(errorStr);
    } else {
      // Set busy state to show spinner
      m_busy = true;
      m_spinnerStartTime = (double)clock() / CLOCKS_PER_SEC;
      SetAPIStatus("Comparing tracks...", 0xFFFF66FF); // Yellow
    }

    return true;
  }

  // Single-track mode (original behavior)
  // Find the track type (next word)
  size_t typeEnd = afterMix.find(' ');
  std::string trackType;
  std::string userQuery;

  if (typeEnd == std::string::npos) {
    trackType = afterMix;
    userQuery = "";
  } else {
    trackType = afterMix.substr(0, typeEnd);
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

void MagdaImGuiChat::StartDirectOpenAIRequest(const std::string &question) {
  // Get REAPER state snapshot on main thread
  char *stateJson = MagdaState::GetStateSnapshot();
  std::string stateStr = stateJson ? stateJson : "{}";
  if (stateJson) {
    free(stateJson);
  }

  // Store the pending state
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    m_pendingQuestion = question;
    m_asyncPending = true;
    m_asyncResultReady = false;
    m_asyncSuccess = false;
    m_cancelRequested = false;
    m_directOpenAI = true;  // Mark as direct OpenAI (DSL result)
    m_asyncResponseJson.clear();
    m_asyncErrorMsg.clear();
    m_streamingActions.clear();
  }

  // Wait for any previous thread to finish
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }

  // Start async thread for agent orchestration
  m_asyncThread = std::thread([this, question, stateStr]() {
    MagdaAgentManager *agentMgr = GetMagdaAgentManager();
    if (!agentMgr || !agentMgr->HasAPIKey()) {
      // Fallback to direct OpenAI client
      MagdaOpenAI *openai = GetMagdaOpenAI();
      if (!openai || !openai->HasAPIKey()) {
        std::lock_guard<std::mutex> lock(m_asyncMutex);
        m_asyncSuccess = false;
        m_asyncErrorMsg = "No API key configured";
        m_asyncResultReady = true;
        m_asyncPending = false;
        return;
      }

      // Use simple DAW-only mode via OpenAI client
      WDL_FastString dslCode, errorMsg;
      bool success = openai->GenerateDSLWithState(
          question.c_str(), MAGDA_DSL_TOOL_DESCRIPTION, stateStr.c_str(), dslCode, errorMsg);

      std::lock_guard<std::mutex> lock(m_asyncMutex);
      m_asyncSuccess = success && dslCode.GetLength() > 0;
      m_asyncResponseJson = dslCode.Get();
      m_asyncErrorMsg = errorMsg.Get();
      m_asyncResultReady = true;
      m_asyncPending = false;
      return;
    }

    // Use agent orchestration (detects and runs appropriate agents)
    std::vector<AgentResult> results;
    WDL_FastString errorMsg;
    bool success = agentMgr->Orchestrate(question.c_str(), stateStr.c_str(), results, errorMsg);

    if (success && !results.empty()) {
      // Combine all DSL results
      std::string combinedDSL;
      for (const auto& result : results) {
        if (result.success && !result.dslCode.empty()) {
          if (!combinedDSL.empty()) combinedDSL += "\n";
          combinedDSL += result.dslCode;
        }
      }

      std::lock_guard<std::mutex> lock(m_asyncMutex);
      m_asyncSuccess = !combinedDSL.empty();
      m_asyncResponseJson = combinedDSL;
      m_asyncErrorMsg = m_asyncSuccess ? "" : "No DSL generated";
      m_asyncResultReady = true;
      m_asyncPending = false;
    } else {
      std::lock_guard<std::mutex> lock(m_asyncMutex);
      m_asyncSuccess = false;
      m_asyncErrorMsg = errorMsg.GetLength() > 0 ? errorMsg.Get() : "Agent orchestration failed";
      m_asyncResultReady = true;
      m_asyncPending = false;
    }
  });
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
  ClearStreamingBuffer();                // Clear any previous streaming content

  // Reset mix analysis phase for regular chat requests (show generic "Processing...")
  MagdaBounceWorkflow::SetCurrentPhase(MIX_PHASE_IDLE);

  // Check if OpenAI API key is configured - use direct OpenAI if available
  MagdaOpenAI* openai = GetMagdaOpenAI();
  if (openai && openai->HasAPIKey()) {
    // Use direct OpenAI API
    SetAPIStatus("OpenAI Direct", 0x88FF88FF); // Green
    StartDirectOpenAIRequest(question);
    return;
  }

  // Fall back to Go backend
  SetAPIStatus("Connected", 0x88FF88FF); // Green - API is healthy

  // Set backend URL from settings
  const char *backendUrl = MagdaImGuiLogin::GetBackendURL();
  if (backendUrl && backendUrl[0]) {
    s_httpClient.SetBackendURL(backendUrl);
  }

  // Only set JWT token if auth is required (Gateway mode)
  // Local API (AuthMode::None) doesn't need authentication
  if (g_imguiLogin && g_imguiLogin->GetAuthMode() == AuthMode::Gateway) {
    const char *token = MagdaImGuiLogin::GetStoredToken();
    if (token && token[0]) {
      s_httpClient.SetJWTToken(token);
    }
  } else {
    // Clear any existing token for local mode
    s_httpClient.SetJWTToken(nullptr);
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
    m_cancelRequested = false;  // Reset cancel flag for new request
    m_asyncResponseJson.clear();
    m_asyncErrorMsg.clear();
    m_streamingActions.clear(); // Clear any pending actions
  }

  // Wait for any previous thread to finish
  if (m_asyncThread.joinable()) {
    m_asyncThread.join();
  }

  // Streaming context for callback
  struct StreamContext {
    MagdaImGuiChat *chat;
    std::vector<std::string> allActions;
    int actionCount;
  };
  StreamContext *ctx = new StreamContext{this, {}, 0};

  // Start streaming request thread
  m_asyncThread = std::thread([requestJsonStr, ctx]() {
    WDL_FastString error_msg;

    // Streaming callback - must be capture-less lambda or static function
    auto streamCallback = [](const char *event_json, void *user_data) {
      StreamContext *ctx = static_cast<StreamContext *>(user_data);
      if (!ctx || !ctx->chat)
        return;

      // Check if request was cancelled
      {
        std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
        if (ctx->chat->m_cancelRequested) {
          return;  // Stop processing if cancelled
        }
      }

      // Parse event JSON
      wdl_json_parser parser;
      wdl_json_element *root =
          parser.parse(event_json, (int)strlen(event_json));

      if (!parser.m_err && root) {
        // Check event type
        wdl_json_element *type_elem = root->get_item_by_name("type");
        if (type_elem && type_elem->m_value_string) {
          const char *eventType = type_elem->m_value;

          if (strcmp(eventType, "action") == 0) {
            // The API client already extracted the action object from the SSE
            // wrapper So event_json IS the action object:
            // {"action":"create_track",...}
            std::string actionEventJson = event_json;

            // Debug log
            if (g_rec) {
              void (*ShowConsoleMsg)(const char *msg) =
                  (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
              if (ShowConsoleMsg) {
                char log_msg[512];
                snprintf(
                    log_msg, sizeof(log_msg),
                    "MAGDA: Chat callback received action type event: %.200s\n",
                    event_json);
                ShowConsoleMsg(log_msg);
              }
            }

            // Note: action_elem check is legacy - the action is already
            // extracted
            wdl_json_element *action_elem = root->get_item_by_name("action");
            if (action_elem) {

              {
                std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
                ctx->chat->m_streamingActions.push_back(actionEventJson);
              }
              ctx->allActions.push_back(actionEventJson);
              ctx->actionCount++;

              // Update streaming buffer to show formatted action
              {
                std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
                // The actionEventJson IS the action object itself (already
                // extracted by API client) e.g.
                // {"action":"create_track","index":0,"name":"bass"}
                wdl_json_parser actionParser;
                wdl_json_element *actionObj = actionParser.parse(
                    actionEventJson.c_str(), (int)actionEventJson.length());
                if (!actionParser.m_err && actionObj) {
                  // Format the action object directly
                  std::string formatted =
                      FormatAction(actionObj, ctx->actionCount - 1);
                  if (!formatted.empty()) {
                    ctx->chat->m_streamingBuffer += formatted + "\n";
                  } else {
                    // Fallback to progress message
                    char progress_msg[256];
                    snprintf(progress_msg, sizeof(progress_msg),
                             "Received action %d...\n", ctx->actionCount);
                    ctx->chat->m_streamingBuffer += progress_msg;
                  }
                } else {
                  // Fallback to progress message
                  char progress_msg[256];
                  snprintf(progress_msg, sizeof(progress_msg),
                           "Received action %d...\n", ctx->actionCount);
                  ctx->chat->m_streamingBuffer += progress_msg;
                }
              }
            }
          } else if (strcmp(eventType, "done") == 0) {
            // Streaming complete
            {
              std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
              ctx->chat->m_asyncSuccess = true;
              ctx->chat->m_asyncResultReady = true;
              ctx->chat->m_asyncPending = false;
              // Build final response JSON with all actions
              ctx->chat->m_asyncResponseJson = "{\"actions\":[";
              for (size_t i = 0; i < ctx->allActions.size(); i++) {
                if (i > 0)
                  ctx->chat->m_asyncResponseJson += ",";
                // Extract action from event JSON
                wdl_json_parser p;
                wdl_json_element *r = p.parse(ctx->allActions[i].c_str(),
                                              (int)ctx->allActions[i].length());
                if (!p.m_err && r) {
                  wdl_json_element *a = r->get_item_by_name("action");
                  if (a && a->m_value_string) {
                    ctx->chat->m_asyncResponseJson += a->m_value;
                  } else {
                    // Fallback: use the action JSON directly if nested
                    // extraction fails
                    ctx->chat->m_asyncResponseJson += ctx->allActions[i];
                  }
                }
              }
              ctx->chat->m_asyncResponseJson += "]}";
            }
            return; // Don't queue "done" as an action
          } else if (strcmp(eventType, "error") == 0) {
            // Error event
            const char *errorMsg = "Unknown error";
            wdl_json_element *msg_elem = root->get_item_by_name("message");
            if (msg_elem && msg_elem->m_value_string) {
              errorMsg = msg_elem->m_value;
            }
            {
              std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
              ctx->chat->m_asyncErrorMsg = errorMsg;
              ctx->chat->m_asyncSuccess = false;
              ctx->chat->m_asyncResultReady = true;
              ctx->chat->m_asyncPending = false;
            }
            return;
          }
        } else {
          // No type field - this IS the action JSON extracted by the API client
          // e.g. {"action":"create_track","index":0,"name":"bass"}
          std::string actionJson = event_json;

          {
            std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
            ctx->chat->m_streamingActions.push_back(actionJson);

            // Format and add to streaming buffer for display
            std::string formatted = FormatAction(root, ctx->actionCount);
            if (!formatted.empty()) {
              ctx->chat->m_streamingBuffer += formatted + "\n";
            } else {
              char progress_msg[256];
              snprintf(progress_msg, sizeof(progress_msg), "Action %d: %s\n",
                       ctx->actionCount + 1,
                       root->get_string_by_name("action")
                           ? root->get_string_by_name("action")
                           : "unknown");
              ctx->chat->m_streamingBuffer += progress_msg;
            }
          }
          ctx->allActions.push_back(actionJson);
          ctx->actionCount++;
        }
      } else {
        // Parse failed - might still be valid JSON, try queuing it
        {
          std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
          ctx->chat->m_streamingActions.push_back(std::string(event_json));

          // Still add to buffer for visibility
          char progress_msg[256];
          snprintf(progress_msg, sizeof(progress_msg), "Action %d received\n",
                   ctx->actionCount + 1);
          ctx->chat->m_streamingBuffer += progress_msg;
        }
        ctx->allActions.push_back(std::string(event_json));
        ctx->actionCount++;
      }
    };

    // Make streaming request to /api/v1/chat/stream
    bool success = s_httpClient.SendPOSTStream(
        "/api/v1/chat/stream", requestJsonStr.c_str(), streamCallback, ctx,
        error_msg, 60);

    // If streaming failed completely (not just an error event)
    if (!success) {
      {
        std::lock_guard<std::mutex> lock(ctx->chat->m_asyncMutex);
        ctx->chat->m_asyncSuccess = false;
        ctx->chat->m_asyncErrorMsg = error_msg.Get();
        ctx->chat->m_asyncResultReady = true;
        ctx->chat->m_asyncPending = false;
      }
    }
    // Otherwise, success/error was already set by the callback

    // Clean up context
    delete ctx;
  });
}

void MagdaImGuiChat::ProcessAsyncResult() {
  // First check for mix analysis streaming state (TRUE STREAMING)
  {
    MixStreamingState streamState;
    if (MagdaBounceWorkflow::GetStreamingState(streamState)) {
      // Update the assistant message with streamed content
      if (streamState.isStreaming || streamState.streamComplete) {
        // Create or update streaming message
        if (m_history.empty() || m_history.back().is_user ||
            !m_isMixAnalysisStreaming) {
          // Start new message
          AddAssistantMessage("");
          m_isMixAnalysisStreaming = true;
          m_lastMixStreamBuffer.clear();
        }

        // Update message content with accumulated stream
        if (!m_history.empty() && streamState.streamBuffer != m_lastMixStreamBuffer) {
          m_history.back().content = streamState.streamBuffer;
          m_lastMixStreamBuffer = streamState.streamBuffer;
          m_scrollToBottom = true;
        }
      }

      if (streamState.streamComplete) {
        // Streaming finished
        m_isMixAnalysisStreaming = false;

        if (streamState.streamError) {
          std::string errorStr = "Mix analysis error: ";
          errorStr += streamState.errorMessage;
          if (!m_history.empty()) {
            m_history.back().content = errorStr;
          }
          SetAPIStatus("Error", 0xFF6666FF); // Red
        } else {
          SetAPIStatus("Connected", 0x88FF88FF); // Green
        }
        m_busy = false;

        // Clear streaming state for next request
        MagdaBounceWorkflow::ClearStreamingState();
        return;
      }

      // Still streaming - don't return, let UI update
      if (streamState.isStreaming) {
        return;
      }
    }
  }

  // Fallback: check for non-streaming mix analysis results
  {
    MixAnalysisResult mixResult;
    if (MagdaBounceWorkflow::GetPendingResult(mixResult)) {
      MagdaBounceWorkflow::ClearPendingResult();

      if (mixResult.success) {
        // Non-streaming: show full text at once
        AddAssistantMessage(mixResult.responseText);
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

  // Process streaming actions as they arrive (even if stream not complete)
  // This allows actions to execute in real-time as they stream in
  std::vector<std::string> actionsToExecute;
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    if (!m_streamingActions.empty()) {
      actionsToExecute = m_streamingActions;
      m_streamingActions.clear(); // Clear after copying
    }
  }

  // Execute actions from stream on MAIN thread
  for (const auto &actionEventJson : actionsToExecute) {
    // The actionEventJson is already the unwrapped action object:
    // {"action":"create_track","index":0,"instrument":"@plugin:serum_2","name":"bass"}
    // Just wrap it in an array for ExecuteActions
    std::string singleActionJson = "[" + actionEventJson + "]";

    // Debug: log what we're executing
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[1024];
        snprintf(log_msg, sizeof(log_msg), "MAGDA: Executing action: %.500s\n",
                 singleActionJson.c_str());
        ShowConsoleMsg(log_msg);
      }
    }

    WDL_FastString execution_result, execution_error;
    if (!MagdaActions::ExecuteActions(singleActionJson.c_str(),
                                      execution_result, execution_error)) {
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
    } else {
      // Log success
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Action executed successfully\n");
        }
      }
    }
  }

  // Check if final result is ready (stream complete)
  bool resultReady = false;
  bool success = false;
  std::string responseJson;
  std::string errorMsg;

  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    if (!m_asyncResultReady) {
      // Stream still in progress, but we've executed any queued actions
      // API is connected and working - don't expose streaming implementation
      // detail
      SetAPIStatus("Connected", 0x88FF88FF); // Green - API is healthy
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

  // Check if this is a direct OpenAI (DSL) result
  bool isDSL = false;
  {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    isDSL = m_directOpenAI;
    m_directOpenAI = false;  // Reset for next request
  }

  // Process final result on the MAIN thread
  if (success) {
    // For direct OpenAI: execute DSL code
    if (isDSL && !responseJson.empty()) {
      // Log the DSL code we received
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char log_msg[2048];
          snprintf(log_msg, sizeof(log_msg), "MAGDA: OpenAI generated DSL:\n%s\n",
                   responseJson.c_str());
          ShowConsoleMsg(log_msg);
        }
      }

      // Execute the DSL code - route each line to appropriate interpreter
      bool dslSuccess = true;
      std::string lastError;
      int successCount = 0;
      std::vector<std::string> actionSummaries; // Track what actions were performed

      // Clear DSL context before processing
      MagdaDSLContext::Get().Clear();

      // Split DSL by newlines and SORT: DAW commands first, then content commands
      // This ensures track/clip creation happens before MIDI notes are added
      std::string dslCode = responseJson;
      std::vector<std::string> dawCommands;     // track, clip, fx - execute first
      std::vector<std::string> contentCommands; // arpeggio, chord, pattern - execute second
      std::string jsfxCode; // JSFX is special - entire block

      size_t pos = 0;
      while (pos < dslCode.size()) {
        size_t endPos = dslCode.find('\n', pos);
        if (endPos == std::string::npos) endPos = dslCode.size();

        std::string line = dslCode.substr(pos, endPos - pos);
        pos = endPos + 1;

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r");
        line = line.substr(start, end - start + 1);
        if (line.empty()) continue;

        // Categorize command
        if (line.find("desc:") == 0 || line.find("@init") != std::string::npos ||
            line.find("@sample") != std::string::npos) {
          jsfxCode = dslCode; // JSFX is entire block
          break;
        } else if (line.find("track(") == 0 || line.find("clip(") == 0 ||
                   line.find("fx(") == 0 || line.find("item(") == 0) {
          dawCommands.push_back(line);
        } else if (line.find("arpeggio(") == 0 || line.find("chord(") == 0 ||
                   line.find("note(") == 0 || line.find("progression(") == 0 ||
                   line.find("pattern(") == 0) {
          contentCommands.push_back(line);
        } else {
          // Unknown - treat as DAW command (fallback)
          dawCommands.push_back(line);
        }
      }

      // Helper to extract a human-readable summary from a DSL command
      auto getActionSummary = [](const std::string& line) -> std::string {
        // Extract key info from common patterns
        if (line.find("track(") == 0) {
          // track(name="X").new() or track(name="X").new_clip(...)
          size_t nameStart = line.find("name=\"");
          if (nameStart != std::string::npos) {
            nameStart += 6;
            size_t nameEnd = line.find("\"", nameStart);
            if (nameEnd != std::string::npos) {
              std::string name = line.substr(nameStart, nameEnd - nameStart);
              if (line.find(".new_clip") != std::string::npos) {
                return "Created track '" + name + "' with clip";
              } else {
                return "Created track '" + name + "'";
              }
            }
          }
          return "Created track";
        } else if (line.find("arpeggio(") == 0) {
          size_t symStart = line.find("symbol=");
          if (symStart != std::string::npos) {
            symStart += 7;
            size_t symEnd = line.find_first_of(",)", symStart);
            if (symEnd != std::string::npos) {
              std::string sym = line.substr(symStart, symEnd - symStart);
              return "Added " + sym + " arpeggio";
            }
          }
          return "Added arpeggio";
        } else if (line.find("chord(") == 0) {
          size_t symStart = line.find("symbol=");
          if (symStart != std::string::npos) {
            symStart += 7;
            size_t symEnd = line.find_first_of(",)", symStart);
            if (symEnd != std::string::npos) {
              std::string sym = line.substr(symStart, symEnd - symStart);
              return "Added " + sym + " chord";
            }
          }
          return "Added chord";
        } else if (line.find("pattern(") == 0) {
          return "Added drum pattern";
        } else if (line.find("progression(") == 0) {
          return "Added chord progression";
        } else if (line.find("note(") == 0) {
          return "Added note";
        } else if (line.find("fx(") == 0 || line.find(".add_fx") != std::string::npos) {
          return "Added FX";
        } else if (line.find("clip(") == 0 || line.find(".new_clip") != std::string::npos) {
          return "Created clip";
        }
        return "";
      };

      // Helper lambda to execute a line
      auto executeLine = [&](const std::string& line) -> bool {
        bool lineSuccess = false;
        if (line.find("arpeggio(") == 0 || line.find("chord(") == 0 ||
            line.find("note(") == 0 || line.find("progression(") == 0) {
          MagdaArranger::Interpreter arrangerInterp;
          lineSuccess = arrangerInterp.Execute(line.c_str());
          if (!lineSuccess) lastError = arrangerInterp.GetError();
        } else if (line.find("pattern(") == 0) {
          MagdaDrummer::Interpreter drummerInterp;
          lineSuccess = drummerInterp.Execute(line.c_str());
          if (!lineSuccess) lastError = drummerInterp.GetError();
        } else {
          MagdaDSL::Interpreter dawInterp;
          lineSuccess = dawInterp.Execute(line.c_str());
          if (!lineSuccess) lastError = dawInterp.GetError();
        }
        // Add action summary if successful
        if (lineSuccess) {
          std::string summary = getActionSummary(line);
          if (!summary.empty()) {
            actionSummaries.push_back(summary);
          }
        }
        return lineSuccess;
      };

      // Handle JSFX separately
      if (!jsfxCode.empty()) {
        MagdaJSFX::Interpreter jsfxInterp;
        jsfxInterp.SetTargetTrack(-1);
        if (jsfxInterp.Execute(jsfxCode.c_str())) {
          successCount++;
          actionSummaries.push_back("Created JSFX effect");
        } else {
          lastError = jsfxInterp.GetError();
          dslSuccess = false;
        }
      } else {
        // Execute DAW commands FIRST (creates tracks/clips)
        for (const auto& cmd : dawCommands) {
          if (executeLine(cmd)) {
            successCount++;
          } else {
            dslSuccess = false;
          }
        }

        // Execute content commands SECOND (adds MIDI to created tracks)
        for (const auto& cmd : contentCommands) {
          if (executeLine(cmd)) {
            successCount++;
          } else {
            dslSuccess = false;
          }
        }
      }

      if (successCount > 0 && !dslSuccess) {
        // Partial success - show what worked and the error
        std::string msg;
        for (const auto& summary : actionSummaries) {
          msg += "✓ " + summary + "\n";
        }
        msg += "⚠ Error: " + lastError;
        AddAssistantMessage(msg);
        SetAPIStatus("Partial", 0xFFAA44FF); // Orange
      } else if (dslSuccess && !actionSummaries.empty()) {
        // Full success - show all actions performed
        std::string msg;
        for (size_t i = 0; i < actionSummaries.size(); i++) {
          msg += "✓ " + actionSummaries[i];
          if (i < actionSummaries.size() - 1) msg += "\n";
        }
        AddAssistantMessage(msg);
        SetAPIStatus("Done", 0x88FF88FF); // Green
      } else if (dslSuccess) {
        // Success but no trackable actions (fallback)
        AddAssistantMessage("Done.");
        SetAPIStatus("Done", 0x88FF88FF); // Green
      } else {
        std::string errorStr = "Error: " + lastError;
        AddAssistantMessage(errorStr);
        SetAPIStatus("Error", 0xFF6666FF); // Red
      }

      // Clear DSL context after processing
      MagdaDSLContext::Get().Clear();
    } else {
      // For streaming from Go backend: the buffer should contain formatted action messages
      // ClearStreamingBuffer adds buffer content to chat history
      bool hadStreamingContent = !m_streamingBuffer.empty();
      ClearStreamingBuffer();

      // Only add summary if streaming buffer was empty (non-streaming response)
      // For streaming, the actions were already formatted and displayed
      if (!hadStreamingContent) {
        // Extract summary of what was done from the response JSON
        std::string summary = ExtractActionSummary(responseJson.c_str());
        if (!summary.empty()) {
          AddAssistantMessage(summary);
        } else {
          // Fallback: show action count
          char summary_msg[256];
          int action_count = 0;
          if (!responseJson.empty()) {
            char *actions_json = MagdaHTTPClient::ExtractActionsJSON(
                responseJson.c_str(), (int)responseJson.length());
            if (actions_json) {
              // Count actions (rough estimate)
              for (const char *p = actions_json; *p; p++) {
                if (strncmp(p, "\"action\":", 9) == 0) {
                  action_count++;
                }
              }
              free(actions_json);
            }
          }
          if (action_count > 0) {
            snprintf(summary_msg, sizeof(summary_msg), "Executed %d action(s).",
                     action_count);
            AddAssistantMessage(summary_msg);
          } else {
            AddAssistantMessage("Done.");
          }
        }
      }
      SetAPIStatus("Connected", 0x88FF88FF); // Green
    }
  } else {
    // Error - show more context
    ClearStreamingBuffer();
    std::string errorStr = "Error: ";
    errorStr += errorMsg;
    AddAssistantMessage(errorStr);
    SetAPIStatus("Error", 0xFF6666FF); // Red
  }

  m_busy = false;
}
