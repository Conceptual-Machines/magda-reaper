#pragma once

#include "reaper_plugin.h"

// Chat window class with two panes: request (input) and response (output)
// Based on SWS window pattern
class MagdaChatWindow {
public:
  MagdaChatWindow();
  ~MagdaChatWindow();

  void Show(bool toggle = false);
  void Hide();
  bool IsVisible() const {
    return m_hwnd != nullptr && IsWindowVisible(m_hwnd);
  }

  // Check API health and update status
  void CheckAPIHealth();

private:
  HWND m_hwnd;
  HWND m_hwndQuestionInput;   // Input field for requests
  HWND m_hwndQuestionDisplay; // Display area for requests (left pane)
  HWND m_hwndReplyDisplay;    // Display area for responses (right pane)
  HWND m_hwndSendButton;
  HWND m_hwndRequestHeader;  // "REQUEST" header label
  HWND m_hwndResponseHeader; // "RESPONSE" header label
  HWND m_hwndStatusFooter;   // Status footer with API health

  int m_requestLineCount;  // Track request line count for alignment
  int m_responseLineCount; // Track response line count for alignment

  // SWS pattern: static proc that gets 'this' from GWLP_USERDATA
  static INT_PTR WINAPI sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

  // Instance proc that handles all messages
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCommand(int command, int notifyCode);
  void OnSendMessage();
  void AddRequest(const char *request);
  void AddResponse(const char *response);
  void AlignRequestWithResponse(); // Add blank lines to align columns
  void UpdateLayout(int width, int height);
  void UpdateStatus(const char *status, bool isOK);
};
