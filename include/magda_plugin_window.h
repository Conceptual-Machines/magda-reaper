#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// Plugin alias management window
class MagdaPluginWindow {
public:
  MagdaPluginWindow();
  ~MagdaPluginWindow();

  void Show(bool toggle = false);
  void Hide();
  bool IsVisible() const {
    return m_hwnd != nullptr && IsWindowVisible(m_hwnd);
  }

  HWND m_hwnd;
  HWND m_hwndAliasList;     // List view or list box showing aliases
  HWND m_hwndScanButton;    // Button to scan plugins
  HWND m_hwndRefreshButton; // Button to refresh alias list

private:
  // SWS pattern: static proc that gets 'this' from GWLP_USERDATA
  static INT_PTR WINAPI sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

  // Instance proc that handles all messages
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCommand(int command, int notifyCode);
  void OnScanPlugins();
  void RefreshAliasList();
};
