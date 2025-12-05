#include "magda_settings_window.h"
#include "magda_settings_resource.h"
#include "magda_state.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

MagdaSettingsWindow::MagdaSettingsWindow()
    : m_hwnd(nullptr), m_hwndBackendURLInput(nullptr),
      m_hwndSaveButton(nullptr), m_hwndStateFilterModeCombo(nullptr),
      m_hwndIncludeEmptyTracksCheck(nullptr),
      m_hwndMaxClipsPerTrackInput(nullptr) {}

MagdaSettingsWindow::~MagdaSettingsWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

void MagdaSettingsWindow::Show(bool toggle) {
  if (!g_rec || !g_hInst)
    return;

  if (m_hwnd && IsWindowVisible(m_hwnd)) {
    if (toggle) {
      Hide();
    } else {
      SetForegroundWindow(m_hwnd);
      SetFocus(m_hwndBackendURLInput);
    }
    return;
  }

  if (!m_hwnd) {
    // Create a modeless dialog - SWS pattern
    m_hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MAGDA_SETTINGS),
                               NULL, sDialogProc, (LPARAM)this);
  }

  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);

    // Ensure controls are initialized
    if (!m_hwndBackendURLInput || !m_hwndSaveButton) {
      m_hwndBackendURLInput = GetDlgItem(m_hwnd, IDC_BACKEND_URL_INPUT);
      m_hwndSaveButton = GetDlgItem(m_hwnd, IDC_SAVE_BUTTON);
      m_hwndStateFilterModeCombo =
          GetDlgItem(m_hwnd, IDC_STATE_FILTER_MODE_COMBO);
      m_hwndIncludeEmptyTracksCheck =
          GetDlgItem(m_hwnd, IDC_INCLUDE_EMPTY_TRACKS_CHECK);
      m_hwndMaxClipsPerTrackInput =
          GetDlgItem(m_hwnd, IDC_MAX_CLIPS_PER_TRACK_INPUT);
    }

    SetFocus(m_hwndBackendURLInput);
  }
}

void MagdaSettingsWindow::Hide() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

INT_PTR WINAPI MagdaSettingsWindow::sDialogProc(HWND hwnd, UINT uMsg,
                                                WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_INITDIALOG) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
  }
  MagdaSettingsWindow *pThis =
      (MagdaSettingsWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (pThis) {
    pThis->m_hwnd = hwnd; // Store HWND in instance
    return pThis->DialogProc(uMsg, wParam, lParam);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR MagdaSettingsWindow::DialogProc(UINT uMsg, WPARAM wParam,
                                        LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    // Get control handles
    m_hwndBackendURLInput = GetDlgItem(m_hwnd, IDC_BACKEND_URL_INPUT);
    m_hwndSaveButton = GetDlgItem(m_hwnd, IDC_SAVE_BUTTON);
    m_hwndStateFilterModeCombo =
        GetDlgItem(m_hwnd, IDC_STATE_FILTER_MODE_COMBO);
    m_hwndIncludeEmptyTracksCheck =
        GetDlgItem(m_hwnd, IDC_INCLUDE_EMPTY_TRACKS_CHECK);
    m_hwndMaxClipsPerTrackInput =
        GetDlgItem(m_hwnd, IDC_MAX_CLIPS_PER_TRACK_INPUT);

    // Validate all controls were created
    if (!m_hwndBackendURLInput || !m_hwndSaveButton ||
        !m_hwndStateFilterModeCombo || !m_hwndIncludeEmptyTracksCheck ||
        !m_hwndMaxClipsPerTrackInput) {
      return FALSE;
    }

    // Set initial focus to URL field
    SetFocus(m_hwndBackendURLInput);

    // Load and populate saved backend URL if available
    WDL_FastString stored_url;
    LoadBackendURL(stored_url);
    if (stored_url.GetLength() > 0) {
      SetWindowText(m_hwndBackendURLInput, stored_url.Get());
    } else {
      // Set default URL as placeholder
      SetWindowText(m_hwndBackendURLInput, "https://api.musicalaideas.com");
    }

    // Populate state filter mode combo
    PopulateFilterModeCombo();

    // Load and populate state filter preferences
    StateFilterPreferences prefs;
    LoadStateFilterPreferences(prefs);

    // Set combo selection based on mode
    int comboIndex = 0;
    switch (prefs.mode) {
    case StateFilterMode::All:
      comboIndex = 0;
      break;
    case StateFilterMode::SelectedTracksOnly:
      comboIndex = 1;
      break;
    case StateFilterMode::SelectedTrackClipsOnly:
      comboIndex = 2;
      break;
    case StateFilterMode::SelectedClipsOnly:
      comboIndex = 3;
      break;
    }
    SendMessage(m_hwndStateFilterModeCombo, CB_SETCURSEL, comboIndex, 0);

    // Set checkbox
    SendMessage(m_hwndIncludeEmptyTracksCheck, BM_SETCHECK,
                prefs.includeEmptyTracks ? BST_CHECKED : BST_UNCHECKED, 0);

    // Set max clips per track
    char maxClipsBuf[32];
    if (prefs.maxClipsPerTrack > 0) {
      snprintf(maxClipsBuf, sizeof(maxClipsBuf), "%d", prefs.maxClipsPerTrack);
    } else {
      maxClipsBuf[0] = '0';
      maxClipsBuf[1] = '\0';
    }
    SetWindowText(m_hwndMaxClipsPerTrackInput, maxClipsBuf);

    return TRUE;
  }
  case WM_COMMAND:
    OnCommand(LOWORD(wParam), HIWORD(wParam));
    return 0;
  case WM_CLOSE:
    Hide();
    return 0;
  }
  return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MagdaSettingsWindow::OnCommand(int command, int notifyCode) {
  switch (command) {
  case IDC_SAVE_BUTTON:
    OnSave();
    break;
  case IDOK: // Enter key in input field
    if (m_hwndBackendURLInput && GetFocus() == m_hwndBackendURLInput) {
      OnSave();
    }
    break;
  }
}

void MagdaSettingsWindow::OnSave() {
  if (!m_hwndBackendURLInput)
    return;

  char buffer[512];
  buffer[0] = '\0';
  GetWindowText(m_hwndBackendURLInput, buffer, sizeof(buffer));

  // Store the backend URL (empty string means use default)
  if (strlen(buffer) > 0) {
    StoreBackendURL(buffer);
  } else {
    // Clear the stored URL to use default
    StoreBackendURL("");
  }

  // Store state filter preferences
  StateFilterPreferences prefs;

  // Get filter mode from combo
  int comboIndex =
      (int)SendMessage(m_hwndStateFilterModeCombo, CB_GETCURSEL, 0, 0);
  switch (comboIndex) {
  case 0:
    prefs.mode = StateFilterMode::All;
    break;
  case 1:
    prefs.mode = StateFilterMode::SelectedTracksOnly;
    break;
  case 2:
    prefs.mode = StateFilterMode::SelectedTrackClipsOnly;
    break;
  case 3:
    prefs.mode = StateFilterMode::SelectedClipsOnly;
    break;
  default:
    prefs.mode = StateFilterMode::All;
    break;
  }

  // Get include empty tracks
  prefs.includeEmptyTracks = (SendMessage(m_hwndIncludeEmptyTracksCheck,
                                          BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Get max clips per track
  char maxClipsBuf[32];
  GetWindowText(m_hwndMaxClipsPerTrackInput, maxClipsBuf, sizeof(maxClipsBuf));
  int maxClips = atoi(maxClipsBuf);
  prefs.maxClipsPerTrack = (maxClips > 0) ? maxClips : -1;

  StoreStateFilterPreferences(prefs);

  Hide();
}

void MagdaSettingsWindow::StoreBackendURL(const char *url) {
  if (!g_rec)
    return;
  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (SetExtState) {
    SetExtState("MAGDA", "backend_url", url ? url : "", true);
  }
}

void MagdaSettingsWindow::LoadBackendURL(WDL_FastString &url) {
  if (!g_rec)
    return;
  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (GetExtState) {
    const char *stored_url = GetExtState("MAGDA", "backend_url");
    if (stored_url && strlen(stored_url) > 0) {
      url.Set(stored_url);
    }
  }
}

const char *MagdaSettingsWindow::GetBackendURL() {
  static WDL_FastString s_backend_url;
  LoadBackendURL(s_backend_url);

  // Return stored URL if set, otherwise return default
  if (s_backend_url.GetLength() > 0) {
    return s_backend_url.Get();
  }

  // Check environment variable as fallback
  const char *env_url = getenv("MAGDA_BACKEND_URL");
  if (env_url && strlen(env_url) > 0) {
    return env_url;
  }

  // Return default
  return "https://api.musicalaideas.com";
}

void MagdaSettingsWindow::PopulateFilterModeCombo() {
  if (!m_hwndStateFilterModeCombo)
    return;

  SendMessage(m_hwndStateFilterModeCombo, CB_ADDSTRING, 0,
              (LPARAM) "All tracks and clips");
  SendMessage(m_hwndStateFilterModeCombo, CB_ADDSTRING, 0,
              (LPARAM) "Selected tracks only");
  SendMessage(m_hwndStateFilterModeCombo, CB_ADDSTRING, 0,
              (LPARAM) "Selected tracks + selected clips only");
  SendMessage(m_hwndStateFilterModeCombo, CB_ADDSTRING, 0,
              (LPARAM) "Selected clips only (all tracks)");
}

void MagdaSettingsWindow::StoreStateFilterPreferences(
    const StateFilterPreferences &prefs) {
  if (!g_rec)
    return;

  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (!SetExtState)
    return;

  // Store mode as integer
  char modeStr[32];
  snprintf(modeStr, sizeof(modeStr), "%d", (int)prefs.mode);
  SetExtState("MAGDA", "state_filter_mode", modeStr, true);

  // Store include empty tracks
  SetExtState("MAGDA", "state_filter_include_empty",
              prefs.includeEmptyTracks ? "1" : "0", true);

  // Store max clips per track
  char maxClipsStr[32];
  snprintf(maxClipsStr, sizeof(maxClipsStr), "%d", prefs.maxClipsPerTrack);
  SetExtState("MAGDA", "state_filter_max_clips", maxClipsStr, true);
}

void MagdaSettingsWindow::LoadStateFilterPreferences(
    StateFilterPreferences &prefs) {
  // Set defaults
  prefs.mode = StateFilterMode::All;
  prefs.includeEmptyTracks = true;
  prefs.maxClipsPerTrack = -1;

  if (!g_rec)
    return;

  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (!GetExtState)
    return;

  // Load mode
  const char *modeStr = GetExtState("MAGDA", "state_filter_mode");
  if (modeStr) {
    int mode = atoi(modeStr);
    if (mode >= 0 && mode <= 3) {
      prefs.mode = (StateFilterMode)mode;
    }
  }

  // Load include empty tracks
  const char *includeEmptyStr =
      GetExtState("MAGDA", "state_filter_include_empty");
  if (includeEmptyStr) {
    prefs.includeEmptyTracks = (atoi(includeEmptyStr) != 0);
  }

  // Load max clips per track
  const char *maxClipsStr = GetExtState("MAGDA", "state_filter_max_clips");
  if (maxClipsStr) {
    prefs.maxClipsPerTrack = atoi(maxClipsStr);
  }
}
