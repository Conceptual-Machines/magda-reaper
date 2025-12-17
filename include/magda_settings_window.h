#pragma once

#include "../WDL/WDL/wdlstring.h"
#include "magda_state.h"
#include "reaper_plugin.h"

// Settings window class for MAGDA configuration
// Based on SWS window pattern
class MagdaSettingsWindow {
public:
  MagdaSettingsWindow();
  ~MagdaSettingsWindow();

  void Show(bool toggle = false);
  void Hide();
  bool IsVisible() const {
    return m_hwnd != nullptr && IsWindowVisible(m_hwnd);
  }

  // Get stored backend URL (returns default if not set)
  static const char *GetBackendURL();

  // Load state filter preferences (public so MagdaState can access it)
  static void LoadStateFilterPreferences(StateFilterPreferences &prefs);

private:
  HWND m_hwnd;
  HWND m_hwndBackendURLInput;
  HWND m_hwndSaveButton;

  // State filter controls
  HWND m_hwndStateFilterModeCombo;
  HWND m_hwndIncludeEmptyTracksCheck;
  HWND m_hwndMaxClipsPerTrackInput;

  // SWS pattern: static proc that gets 'this' from GWLP_USERDATA
  static INT_PTR WINAPI sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

  // Instance proc that handles all messages
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCommand(int command, int notifyCode);
  void OnSave();

  // Helper functions to store/load backend URL persistently
  static void StoreBackendURL(const char *url);
  static void LoadBackendURL(WDL_FastString &url);

  // Helper functions to store/load state filter preferences
  static void StoreStateFilterPreferences(const StateFilterPreferences &prefs);

  // Populate filter mode combo box
  void PopulateFilterModeCombo();
};
