#pragma once

#include "reaper_plugin.h"
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declare the plugin scanner for autocomplete
class MagdaPluginScanner;

// Autocomplete suggestion
struct AutocompleteSuggestion {
  std::string alias;       // "serum"
  std::string plugin_name; // "Serum (Xfer Records)"
  std::string plugin_type; // "synth", "fx", etc.
};

// Chat message
struct ChatMessage {
  std::string content;
  bool is_user; // true = user, false = assistant
};

class MagdaImGuiChat {
public:
  MagdaImGuiChat();
  ~MagdaImGuiChat();

  // Initialize ReaImGui function pointers
  // Returns false if ReaImGui is not available
  bool Initialize(reaper_plugin_info_t *rec);

  // Check if ReaImGui is available
  bool IsAvailable() const { return m_available; }

  // Show/hide window
  void Show();
  void Hide();
  bool IsVisible() const { return m_visible; }
  void Toggle();

  // Set input text (for prefilling from external triggers)
  void SetInputText(const char *text);

  // Show window and prefill input
  void ShowWithInput(const char *text);

  // Main render loop - call from timer/defer
  void Render();

  // Set plugin scanner for autocomplete
  void SetPluginScanner(MagdaPluginScanner *scanner) {
    m_pluginScanner = scanner;
  }

  // Callbacks
  using SendCallback = std::function<void(const std::string &message)>;
  void SetOnSend(SendCallback cb) { m_onSend = cb; }

  // Add messages to chat history
  void AddUserMessage(const std::string &msg);
  void AddAssistantMessage(const std::string &msg);
  void AppendStreamingText(const std::string &chunk);
  void ClearStreamingBuffer();

  // Set busy state (waiting for response)
  void SetBusy(bool busy) { m_busy = busy; }
  bool IsBusy() const { return m_busy; }

  // Set API status for footer
  void SetAPIStatus(const std::string &status, int color) {
    m_apiStatus = status;
    m_apiStatusColor = color;
  }

private:
  // ReaImGui function pointers
  void *(*m_ImGui_CreateContext)(const char *label,
                                 int *config_flagsInOptional);
  int (*m_ImGui_ConfigFlags_DockingEnable)();
  bool (*m_ImGui_Begin)(void *ctx, const char *name, bool *p_openInOutOptional,
                        int *flagsInOptional);
  void (*m_ImGui_End)(void *ctx);
  void (*m_ImGui_SetNextWindowSize)(void *ctx, double size_w, double size_h,
                                    int *condInOptional);
  void (*m_ImGui_Text)(void *ctx, const char *text);
  void (*m_ImGui_TextColored)(void *ctx, int col_rgba, const char *text);
  void (*m_ImGui_TextWrapped)(void *ctx, const char *text);
  bool (*m_ImGui_InputText)(void *ctx, const char *label, char *buf, int buf_sz,
                            int *flagsInOptional, void *callbackInOptional);
  bool (*m_ImGui_Button)(void *ctx, const char *label, double *size_wInOptional,
                         double *size_hInOptional);
  void (*m_ImGui_SameLine)(void *ctx, double *offset_from_start_xInOptional,
                           double *spacingInOptional);
  void (*m_ImGui_Separator)(void *ctx);
  bool (*m_ImGui_BeginChild)(void *ctx, const char *str_id,
                             double *size_wInOptional, double *size_hInOptional,
                             int *child_flagsInOptional,
                             int *window_flagsInOptional);
  void (*m_ImGui_EndChild)(void *ctx);
  bool (*m_ImGui_BeginPopup)(void *ctx, const char *str_id,
                             int *flagsInOptional);
  void (*m_ImGui_EndPopup)(void *ctx);
  void (*m_ImGui_OpenPopup)(void *ctx, const char *str_id,
                            int *flagsInOptional);
  void (*m_ImGui_CloseCurrentPopup)(void *ctx);
  bool (*m_ImGui_Selectable)(void *ctx, const char *label,
                             bool *p_selectedInOutOptional,
                             int *flagsInOptional, double *size_wInOptional,
                             double *size_hInOptional);
  bool (*m_ImGui_IsWindowAppearing)(void *ctx);
  void (*m_ImGui_SetKeyboardFocusHere)(void *ctx, int *offsetInOptional);
  double (*m_ImGui_GetScrollY)(void *ctx);
  double (*m_ImGui_GetScrollMaxY)(void *ctx);
  void (*m_ImGui_SetScrollHereY)(void *ctx, double *center_y_ratioInOptional);
  int (*m_ImGui_GetKeyMods)(void *ctx);
  bool (*m_ImGui_IsKeyPressed)(void *ctx, int key, bool *repeatInOptional);
  void (*m_ImGui_PushStyleColor)(void *ctx, int idx, int col_rgba);
  void (*m_ImGui_PopStyleColor)(void *ctx, int *countInOptional);
  // Dock-related functions
  bool (*m_ImGui_BeginPopupContextWindow)(void *ctx,
                                          const char *str_idInOptional,
                                          int *popup_flagsInOptional);
  bool (*m_ImGui_IsWindowDocked)(void *ctx);
  void (*m_ImGui_SetNextWindowDockID)(void *ctx, int dock_id,
                                      int *condInOptional);
  bool (*m_ImGui_MenuItem)(void *ctx, const char *label,
                           const char *shortcutInOptional,
                           bool *p_selectedInOptional, bool *enabledInOptional);
  // Table/column functions
  bool (*m_ImGui_BeginTable)(void *ctx, const char *str_id, int column,
                             int *flagsInOptional,
                             double *outer_size_wInOptional,
                             double *outer_size_hInOptional,
                             double *inner_widthInOptional);
  void (*m_ImGui_EndTable)(void *ctx);
  void (*m_ImGui_TableNextRow)(void *ctx, int *row_flagsInOptional,
                               double *min_row_heightInOptional);
  bool (*m_ImGui_TableNextColumn)(void *ctx);
  void (*m_ImGui_TableSetupColumn)(void *ctx, const char *label,
                                   int *flagsInOptional,
                                   double *init_width_or_weightInOptional,
                                   int *user_idInOptional);
  void (*m_ImGui_TableHeadersRow)(void *ctx);
  // Layout helpers
  void (*m_ImGui_GetContentRegionAvail)(void *ctx, double *wOut, double *hOut);
  void (*m_ImGui_Dummy)(void *ctx, double size_w, double size_h);

  // State
  bool m_available = false;
  bool m_visible = false;
  bool m_busy = false;
  void *m_ctx = nullptr;

  // Docking state
  int m_pendingDockID = 0;
  bool m_hasPendingDock = false;

  // Chat state
  char m_inputBuffer[4096] = {0};
  std::vector<ChatMessage> m_history;
  std::string m_streamingBuffer;
  bool m_scrollToBottom = false;

  // Pending mix analysis actions (waiting for user confirmation)
  bool m_hasPendingMixActions = false;
  std::string m_pendingMixActionsJson;

  // Input command history (for up/down arrow navigation)
  std::vector<std::string> m_inputHistory;
  int m_inputHistoryIndex = -1;
  std::string m_savedInput; // Saves current input when navigating history

  // Autocomplete state
  bool m_showAutocomplete = false;
  int m_autocompleteIndex = 0;
  size_t m_atPosition = std::string::npos;
  std::string m_autocompletePrefix;
  std::vector<AutocompleteSuggestion> m_suggestions;

  // Plugin scanner for autocomplete
  MagdaPluginScanner *m_pluginScanner = nullptr;

  // Callback
  SendCallback m_onSend;

  // API status
  std::string m_apiStatus = "Checking...";
  int m_apiStatusColor = 0xFFFFFFFF; // White default

  // Loading animation state
  double m_spinnerStartTime = 0.0;

  // Async request state
  std::mutex m_asyncMutex;
  std::thread m_asyncThread;
  bool m_asyncPending = false;
  bool m_asyncResultReady = false;
  bool m_asyncSuccess = false;
  std::string m_asyncResponseJson;
  std::string m_asyncErrorMsg;
  std::string m_pendingQuestion; // Question being processed

  // Internal methods
  void ProcessAsyncResult();
  void StartAsyncRequest(const std::string &question);
  void CheckAPIHealth();
  void RenderHeader();
  void RenderInputArea();
  void RenderMainContent();
  void RenderRequestColumn();
  void RenderResponseColumn();
  void RenderControlsColumn();
  void RenderFooter();
  void RenderAutocompletePopup();

  void DetectAtTrigger();
  void UpdateAutocompleteSuggestions();
  void InsertCompletion(const std::string &alias);
  void RenderMessageWithHighlighting(const std::string &content);
  bool HandleMixCommand(const std::string &msg);
};

// Global instance
extern MagdaImGuiChat *g_imguiChat;
