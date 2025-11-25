#pragma once

#include "reaper_plugin.h"

// Chat window class with two panes: question (input) and reply (output)
// Based on SWS window pattern
class MagdaChatWindow {
public:
  MagdaChatWindow();
  ~MagdaChatWindow();

  void Show(bool toggle = false);
  void Hide();
  bool IsVisible() const { return m_hwnd != nullptr && IsWindowVisible(m_hwnd); }

private:
  HWND m_hwnd;
  HWND m_hwndQuestionInput;   // Input field for questions
  HWND m_hwndQuestionDisplay; // Display area for questions (left/top pane)
  HWND m_hwndReplyDisplay;    // Display area for replies (right/bottom pane)
  HWND m_hwndSendButton;

  // SWS pattern: static proc that gets 'this' from GWLP_USERDATA
  static INT_PTR WINAPI sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

  // Instance proc that handles all messages
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCommand(int command, int notifyCode);
  void OnSendMessage();
  void AddQuestion(const char *question);
  void AddReply(const char *reply);
  void UpdateLayout(int width, int height);
};
