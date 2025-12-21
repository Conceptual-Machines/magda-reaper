#pragma once

#include <functional>
#include <string>
#include <vector>

struct reaper_plugin_info_t;

// Chat message for AI assistant
struct JSFXChatMessage {
  bool is_user;
  std::string content;
  std::string description;  // Description of generated code
  std::string code_block;   // Extracted code from AI response
  bool has_code_block;
  bool streaming_complete = false;  // True when streaming is done
};

// File entry for browser
struct JSFXFileEntry {
  std::string name;
  std::string full_path;
  bool is_directory;
  bool is_expanded;  // For directories
  int depth;
};

class MagdaJSFXEditor {
public:
  MagdaJSFXEditor();
  ~MagdaJSFXEditor();

  bool Initialize(reaper_plugin_info_t *rec);
  void Show();
  void Hide();
  bool IsVisible() const { return m_visible; }
  void Render();

  // Check if ReaImGui is available
  bool IsAvailable() const { return m_available; }

private:
  // Panel rendering
  void RenderFilePanel();
  void RenderEditorPanel();
  void RenderEditorContextMenu();
  void RenderChatPanel();
  void RenderToolbar();

  // File operations
  void RefreshFileList();
  void OpenFile(const std::string &path);
  void SaveCurrentFile();
  void SaveAs(const std::string &path);
  void NewFile();
  void RefreshFXBrowser();
  std::string GetEffectsFolder();

  // AI operations
  void SendToAI(const std::string &message);
  void ApplyCodeBlock(const std::string &code);
  void ProcessAIResponse(const std::string &response);

  // JSFX operations
  void RecompileJSFX();
  void AddToSelectedTrack();
  void AddToTrackAndOpen();
  void OpenInReaperEditor();
  void RenderSaveAsDialog();
  void CreateNewFolder(const std::string &name);

  // Editor helpers
  void InsertText(const std::string &text);
  int GetLineCount();

  // State
  bool m_available = false;
  bool m_visible = false;
  void *m_ctx = nullptr;

  // Save As dialog
  bool m_showSaveAsDialog = false;
  char m_saveAsFilename[256] = {0};

  // File browser
  std::vector<JSFXFileEntry> m_files;
  std::string m_currentFolder;

  // Editor
  std::string m_currentFilePath;
  std::string m_currentFileName;
  char m_editorBuffer[65536];  // 64KB buffer for code
  bool m_modified = false;

  // Chat
  std::vector<JSFXChatMessage> m_chatHistory;
  char m_chatInput[1024];
  bool m_waitingForAI = false;
  double m_spinnerStartTime = 0;

  // ReaImGui function pointers
  void *(*m_ImGui_CreateContext)(const char *, int *) = nullptr;
  void (*m_ImGui_DestroyContext)(void *) = nullptr;
  bool (*m_ImGui_Begin)(void *, const char *, bool *, int *) = nullptr;
  void (*m_ImGui_End)(void *) = nullptr;
  void (*m_ImGui_Text)(void *, const char *) = nullptr;
  void (*m_ImGui_TextWrapped)(void *, const char *) = nullptr;
  void (*m_ImGui_TextColored)(void *, int, const char *) = nullptr;
  bool (*m_ImGui_Button)(void *, const char *, double *, double *) = nullptr;
  bool (*m_ImGui_Selectable)(void *, const char *, bool *, int *, double *,
                             double *) = nullptr;
  bool (*m_ImGui_InputText)(void *, const char *, char *, int, int *,
                            void *) = nullptr;
  bool (*m_ImGui_InputTextMultiline)(void *, const char *, char *, int,
                                     double *, double *, int *, void *) = nullptr;
  void (*m_ImGui_Separator)(void *) = nullptr;
  void (*m_ImGui_SameLine)(void *, double *, double *) = nullptr;
  void (*m_ImGui_Dummy)(void *, double, double) = nullptr;
  bool (*m_ImGui_BeginChild)(void *, const char *, double *, double *, int *,
                             int *) = nullptr;
  void (*m_ImGui_EndChild)(void *) = nullptr;
  void (*m_ImGui_SetNextWindowSize)(void *, double, double, int *) = nullptr;
  void (*m_ImGui_PushStyleColor)(void *, int, int) = nullptr;
  void (*m_ImGui_PopStyleColor)(void *, int *) = nullptr;
  void (*m_ImGui_GetContentRegionAvail)(void *, double *, double *) = nullptr;
  double (*m_ImGui_GetTextLineHeight)(void *) = nullptr;
  void (*m_ImGui_BeginGroup)(void *) = nullptr;
  void (*m_ImGui_EndGroup)(void *) = nullptr;
  bool (*m_ImGui_BeginTable)(void *, const char *, int, int *, double *,
                             double *, double *) = nullptr;
  void (*m_ImGui_EndTable)(void *) = nullptr;
  void (*m_ImGui_TableNextRow)(void *, int *, double *) = nullptr;
  void (*m_ImGui_TableNextColumn)(void *) = nullptr;
  void (*m_ImGui_TableSetupColumn)(void *, const char *, int *, double *,
                                   double *) = nullptr;
  int (*m_ImGui_GetStyleColor)(void *, int) = nullptr;
  void (*m_ImGui_SetCursorPosY)(void *, double) = nullptr;
  double (*m_ImGui_GetCursorPosY)(void *) = nullptr;
  double (*m_ImGui_GetScrollY)(void *) = nullptr;
  void (*m_ImGui_SetScrollY)(void *, double) = nullptr;
  double (*m_ImGui_GetScrollMaxY)(void *) = nullptr;

  // Text wrap control
  void (*m_ImGui_PushTextWrapPos)(void *, double *) = nullptr;
  void (*m_ImGui_PopTextWrapPos)(void *) = nullptr;

  // Popup/context menu functions
  bool (*m_ImGui_BeginPopupContextItem)(void *, const char *, int *) = nullptr;
  bool (*m_ImGui_BeginPopupContextWindow)(void *, const char *, int *) = nullptr;
  bool (*m_ImGui_BeginPopup)(void *, const char *, int *) = nullptr;
  void (*m_ImGui_OpenPopup)(void *, const char *, int *) = nullptr;
  void (*m_ImGui_EndPopup)(void *) = nullptr;
  bool (*m_ImGui_MenuItem)(void *, const char *, const char *, bool *, bool *) = nullptr;
  void (*m_ImGui_CloseCurrentPopup)(void *) = nullptr;

  // Keyboard input functions
  int (*m_ImGui_GetKeyMods)(void *) = nullptr;
  bool (*m_ImGui_IsKeyPressed)(void *, int, bool *) = nullptr;

  reaper_plugin_info_t *m_rec = nullptr;

  // Context menu state
  std::string m_contextMenuTarget;  // File path for context menu actions
};

extern MagdaJSFXEditor *g_jsfxEditor;
