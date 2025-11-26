#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// Login window class for user authentication
// Based on SWS window pattern
class MagdaLoginWindow {
public:
  MagdaLoginWindow();
  ~MagdaLoginWindow();

  void Show(bool toggle = false);
  void Hide();
  bool IsVisible() const { return m_hwnd != nullptr && IsWindowVisible(m_hwnd); }

  // Get stored JWT token (empty if not logged in)
  static const char *GetStoredToken();

  // Store JWT token after successful login
  static void StoreToken(const char *token);

private:
  HWND m_hwnd;
  HWND m_hwndEmailInput;
  HWND m_hwndPasswordInput;
  HWND m_hwndLoginButton;
  HWND m_hwndStatusLabel;
  HWND m_hwndStatusIcon;

  // SWS pattern: static proc that gets 'this' from GWLP_USERDATA
  static INT_PTR WINAPI sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

  // Instance proc that handles all messages
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCommand(int command, int notifyCode);
  void OnLogin();
  void OnLoginWithCredentials(const char *email, const char *password);
  void OnLoginComplete(bool success, const char *token, const char *error);
  void SetStatus(const char *status, bool isError = false);
  void UpdateUIForLoggedInState();
  void UpdateUIForLoggedOutState();

  // Static storage for JWT token (using function-local static to avoid initialization issues)
  static WDL_FastString &GetTokenStorage();
};
