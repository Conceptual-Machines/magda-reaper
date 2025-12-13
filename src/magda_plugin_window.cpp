#include "magda_plugin_window.h"
#include "../WDL/WDL/swell/swell-functions.h"
#include "../WDL/WDL/swell/swell-types.h"
#include "magda_plugin_resource.h"
#include "magda_plugin_scanner.h"
#include <cstdio>
#include <cstring>

#ifndef RGB
#define RGB(r, g, b)                                                           \
  ((int)(((unsigned char)(r) | ((unsigned short)((unsigned char)(g)) << 8)) |  \
         (((unsigned int)(unsigned char)(b)) << 16)))
#endif

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;
extern MagdaPluginScanner *g_pluginScanner;
extern MagdaPluginWindow *g_pluginWindow;

MagdaPluginWindow::MagdaPluginWindow()
    : m_hwnd(nullptr), m_hwndAliasList(nullptr), m_hwndScanButton(nullptr),
      m_hwndRefreshButton(nullptr) {}

MagdaPluginWindow::~MagdaPluginWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

void MagdaPluginWindow::Show(bool toggle) {
  if (!g_rec || !g_hInst)
    return;

  if (m_hwnd && IsWindowVisible(m_hwnd)) {
    if (toggle) {
      Hide();
    } else {
      SetForegroundWindow(m_hwnd);
      SetFocus(m_hwndScanButton);
    }
    return;
  }

  if (!m_hwnd) {
    // Create a modeless dialog - SWS pattern
    m_hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MAGDA_PLUGIN), NULL,
                               sDialogProc, (LPARAM)this);
  }

  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);

    // Ensure controls are initialized
    if (!m_hwndAliasList || !m_hwndScanButton) {
      m_hwndAliasList = GetDlgItem(m_hwnd, IDC_ALIAS_LIST);
      m_hwndScanButton = GetDlgItem(m_hwnd, IDC_SCAN_BUTTON);
      m_hwndRefreshButton = GetDlgItem(m_hwnd, IDC_REFRESH_BUTTON);
    }

    // Ensure all controls are visible
    if (m_hwndAliasList) {
      ShowWindow(m_hwndAliasList, SW_SHOW);
    }
    if (m_hwndScanButton) {
      ShowWindow(m_hwndScanButton, SW_SHOW);
    }
    if (m_hwndRefreshButton) {
      ShowWindow(m_hwndRefreshButton, SW_SHOW);
    }
    HWND statusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
    if (statusLabel) {
      ShowWindow(statusLabel, SW_SHOW);
    }

    // Use global scanner instance
    if (!g_pluginScanner) {
      g_pluginScanner = new MagdaPluginScanner();
      g_pluginScanner->LoadFromCache();
    }

    // Refresh alias list
    RefreshAliasList();

    // Invalidate window on macOS (following SWS pattern)
#ifndef _WIN32
    InvalidateRect(m_hwnd, NULL, TRUE);
#endif

    SetFocus(m_hwndScanButton);
  }
}

void MagdaPluginWindow::Hide() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

INT_PTR WINAPI MagdaPluginWindow::sDialogProc(HWND hwnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_INITDIALOG) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
  }
  MagdaPluginWindow *pThis =
      (MagdaPluginWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (pThis) {
    pThis->m_hwnd = hwnd; // Store HWND in instance
    return pThis->DialogProc(uMsg, wParam, lParam);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR MagdaPluginWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    // Get control handles
    m_hwndAliasList = GetDlgItem(m_hwnd, IDC_ALIAS_LIST);
    m_hwndScanButton = GetDlgItem(m_hwnd, IDC_SCAN_BUTTON);
    m_hwndRefreshButton = GetDlgItem(m_hwnd, IDC_REFRESH_BUTTON);

    // Validate all controls were created
    if (!m_hwndAliasList || !m_hwndScanButton || !m_hwndRefreshButton) {
      // Debug: log which control failed
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(
              msg, sizeof(msg),
              "MAGDA: Failed to get controls: list=%p, scan=%p, refresh=%p\n",
              m_hwndAliasList, m_hwndScanButton, m_hwndRefreshButton);
          ShowConsoleMsg(msg);
        }
      }
      return FALSE;
    }

    // Debug: log ListView creation
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA: ListView handle: %p, IsWindow: %d\n",
                 m_hwndAliasList,
                 m_hwndAliasList ? IsWindow(m_hwndAliasList) : 0);
        ShowConsoleMsg(msg);

        // Check if ListView is visible
        if (m_hwndAliasList) {
          snprintf(msg, sizeof(msg),
                   "MAGDA: ListView visible: %d, enabled: %d\n",
                   IsWindowVisible(m_hwndAliasList),
                   IsWindowEnabled(m_hwndAliasList));
          ShowConsoleMsg(msg);
        }
      }
    }

    // If ListView wasn't created from resource, create it programmatically
    if (!m_hwndAliasList || !IsWindow(m_hwndAliasList)) {
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: ListView not found in resource, creating "
                         "programmatically\n");
        }
      }

      // ListView creation failed - this should not happen if resource is
      // correct

      // Create ListView using SWELL API (if available)
      // Note: We'll need to use a different approach since CreateWindow isn't
      // available For now, just log the error
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Cannot create ListView programmatically - "
                         "need to fix resource\n");
        }
      }
      return FALSE; // Fail initialization if ListView isn't created
    }

    // Set up ListView columns
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;

    // Column 0: Plugin Name (wider for longer names)
    lvc.pszText = (char *)"Plugin Name";
    lvc.cx = 500;
    lvc.iSubItem = 0;
    ListView_InsertColumn(m_hwndAliasList, 0, &lvc);

    // Column 1: Alias (single alias only)
    lvc.pszText = (char *)"Alias";
    lvc.cx = 340;
    lvc.iSubItem = 1;
    ListView_InsertColumn(m_hwndAliasList, 1, &lvc);

    // Set extended ListView style with grid lines (following SWS pattern)
    ListView_SetExtendedListViewStyleEx(
        m_hwndAliasList, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT,
        LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

    // Set ListView colors explicitly (white bg, black text for visibility)
    // This is needed on macOS where GetMainHwnd returns black
    ListView_SetBkColor(m_hwndAliasList, RGB(255, 255, 255));
    ListView_SetTextColor(m_hwndAliasList, RGB(0, 0, 0));
    ListView_SetTextBkColor(m_hwndAliasList, RGB(255, 255, 255));
#ifndef _WIN32
    ListView_SetGridColor(m_hwndAliasList, RGB(200, 200, 200));
#endif

    // Force ListView to be visible
    ShowWindow(m_hwndAliasList, SW_SHOW);

    // Layout: compute positions using client coordinates only (no offset hacks)
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    int dialogHeight = clientRect.bottom - clientRect.top;
    int dialogWidth = clientRect.right - clientRect.left;

    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MAGDA: Dialog client: width=%d, height=%d\n", dialogWidth,
                 dialogHeight);
        ShowConsoleMsg(msg);
      }
    }

    // Constants matching resource, interpreted in client coords
    const int padding = 5;
    const int statusLabelTop = 5;
    const int statusLabelHeight = 20;
    const int listLeft = 20;
    const int listRightPadding = 20; // 900 width - 20 left = 860 list width
    const int buttonHeightResource = 30;

    // Compute rects in client coordinates
    const int statusBottom = statusLabelTop + statusLabelHeight; // 25
    const int listTop = statusBottom; // right after label
    const int listWidth =
        dialogWidth - listLeft - listRightPadding; // 860 at 900 width
    const int buttonsTop =
        dialogHeight - buttonHeightResource - padding;   // stick to bottom
    int listViewHeight = buttonsTop - listTop - padding; // fill remaining space

    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MAGDA: Layout calc - dialogHeight=%d, listTop=%d, "
                 "buttonsTop=%d, listViewHeight=%d\n",
                 dialogHeight, listTop, buttonsTop, listViewHeight);
        ShowConsoleMsg(msg);
      }
    }

    // Place ListView at computed client coords
    const int listViewX = listLeft;
    const int listViewWidth = listWidth;

    // Get button positions from resource
    HWND scanButton = GetDlgItem(m_hwnd, IDC_SCAN_BUTTON);
    HWND refreshButton = GetDlgItem(m_hwnd, IDC_REFRESH_BUTTON);
    int scanButtonX = 20;        // From resource
    int scanButtonWidth = 120;   // From resource
    int refreshButtonX = 150;    // From resource
    int refreshButtonWidth = 80; // From resource

    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MAGDA: ListView CALC - dialogHeight=%d, height=%d, x=%d, "
                 "width=%d\n",
                 dialogHeight, listViewHeight, listViewX, listViewWidth);
        ShowConsoleMsg(msg);
      }
    }

    if (listViewHeight < 50) {
      listViewHeight = 50; // Safety minimum
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(
              msg, sizeof(msg),
              "MAGDA: WARNING - ListView height too small, using minimum 50\n");
          ShowConsoleMsg(msg);
        }
      }
    }

    // Apply ListView position - flip Y for macOS SWELL coordinate inversion
    // SWELL inverts Y axis: low Y appears at bottom, high Y at top
    // To flip: newY = dialogHeight - oldY - controlHeight
    // Add 2px adjustment to push ListView down slightly
    int flippedListTop = dialogHeight - listTop - listViewHeight + 2;
    SetWindowPos(m_hwndAliasList, NULL, listViewX, flippedListTop,
                 listViewWidth, listViewHeight, SWP_NOZORDER | SWP_SHOWWINDOW);

    // Verify final position
    RECT listRect2;
    GetWindowRect(m_hwndAliasList, &listRect2);
    POINT finalTop = {listRect2.left, listRect2.top};
    ScreenToClient(m_hwnd, &finalTop);

    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MAGDA: ListView FINAL - targetTop=%d, clientY=%d, width=%d, "
                 "height=%d\n",
                 listTop, finalTop.y, listViewWidth, listViewHeight);
        ShowConsoleMsg(msg);
      }
    }
    // Position buttons - flip Y for macOS SWELL coordinate inversion
    int flippedButtonY = dialogHeight - buttonsTop - buttonHeightResource;
    if (scanButton) {
      SetWindowPos(scanButton, NULL, scanButtonX, flippedButtonY,
                   scanButtonWidth, buttonHeightResource,
                   SWP_NOZORDER | SWP_SHOWWINDOW);

      // Verify button position
      RECT buttonRect2;
      GetWindowRect(scanButton, &buttonRect2);
      POINT buttonFinalTop = {buttonRect2.left, buttonRect2.top};
      ScreenToClient(m_hwnd, &buttonFinalTop);

      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(
              msg, sizeof(msg),
              "MAGDA: Scan button FINAL - targetTop=%d, actualClientY=%d\n",
              flippedButtonY, buttonFinalTop.y);
          ShowConsoleMsg(msg);
        }
      }
    }
    if (refreshButton) {
      SetWindowPos(refreshButton, NULL, refreshButtonX, flippedButtonY,
                   refreshButtonWidth, buttonHeightResource,
                   SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    // Verify the position was set correctly
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        // Use GetWindowRect + ScreenToClient to verify
        RECT listWindowRect;
        GetWindowRect(m_hwndAliasList, &listWindowRect);
        POINT listTopLeft = {listWindowRect.left, listWindowRect.top};
        ScreenToClient(m_hwnd, &listTopLeft);

        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MAGDA: After SetWindowPos(20,25) - ListView at x=%d, y=%d "
                 "(expected 20, 25)\n",
                 listTopLeft.x, listTopLeft.y);
        ShowConsoleMsg(msg);
      }
    }

    // TEST: Add one hardcoded row first to verify ListView works
    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = (char *)"TEST PLUGIN";
    int itemIndex = ListView_InsertItem(m_hwndAliasList, &lvi);
    ListView_SetItemText(m_hwndAliasList, itemIndex, 1,
                         (char *)"test, alias1, alias2");

    // Log after inserting test row
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "MAGDA: Inserted test row, itemIndex=%d, "
                 "ListView_GetItemCount=%d\n",
                 itemIndex, ListView_GetItemCount(m_hwndAliasList));
        ShowConsoleMsg(msg);
      }
    }

    // Refresh alias list
    RefreshAliasList();

    // Invalidate window on macOS (following SWS pattern)
#ifndef _WIN32
    InvalidateRect(m_hwnd, NULL, TRUE);
#endif

    // Set initial focus to scan button
    SetFocus(m_hwndScanButton);
    return TRUE;
  }
  case WM_ERASEBKGND:
    return TRUE; // Let SWELL handle background painting
  case WM_CTLCOLORDLG:
  case WM_CTLCOLORSTATIC:
    // Forward to REAPER main window for theme colors (exact SWS pattern)
    if (g_rec) {
      HWND (*GetMainHwnd)() = (HWND (*)())g_rec->GetFunc("GetMainHwnd");
      if (GetMainHwnd) {
        HWND mainHwnd = GetMainHwnd();
        if (mainHwnd) {
          return SendMessage(mainHwnd, uMsg, wParam, lParam);
        }
      }
    }
    return 0;
  case WM_COMMAND: {
    int command = LOWORD(wParam);
    int notifyCode = HIWORD(wParam);
    OnCommand(command, notifyCode);
    return TRUE;
  }
  // TODO: Add editing support for aliases column
  // SWELL doesn't support LVN_ENDLABELEDIT, so we'll need to implement
  // editing via double-click or a separate dialog
  case WM_CLOSE: {
    Hide();
    return TRUE;
  }
  case WM_USER + 1: {
    // Scan completed - refresh the alias list and update status
    HWND statusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
    if (statusLabel) {
      if (wParam > 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Scan complete: %d plugins, aliases generated", (int)wParam);
        SetWindowText(statusLabel, msg);
      } else {
        SetWindowText(statusLabel,
                      lParam ? (const char *)lParam : "Scan failed");
      }
    }

    // Re-enable scan button
    if (m_hwndScanButton) {
      EnableWindow(m_hwndScanButton, TRUE);
    }

    // Refresh alias list to show new aliases
    RefreshAliasList();

    // Free error message if allocated
    if (lParam) {
      free((void *)lParam);
    }
    return TRUE;
  }
  }
  return FALSE;
}

void MagdaPluginWindow::OnCommand(int command, int notifyCode) {
  switch (command) {
  case IDC_SCAN_BUTTON:
    OnScanPlugins();
    break;
  case IDC_REFRESH_BUTTON:
    RefreshAliasList();
    break;
  }
}

// Static callback function for async scan - must be a plain C function pointer
// This is called from the background thread, so it uses PostMessage to update
// UI
static void ScanAsyncCallback(bool success, int plugin_count,
                              const char *error) {
  // Post message to update UI on main thread
  if (g_rec && g_pluginWindow) {
    void (*PostMessage)(HWND, UINT, WPARAM, LPARAM) =
        (void (*)(HWND, UINT, WPARAM, LPARAM))g_rec->GetFunc("PostMessage");
    if (PostMessage && g_pluginWindow->m_hwnd) {
      // Use WM_USER + 1 as custom message for scan completion
      PostMessage(g_pluginWindow->m_hwnd, WM_USER + 1,
                  success ? plugin_count : 0,
                  (LPARAM)(error ? strdup(error) : nullptr));
    }
  }
}

void MagdaPluginWindow::OnScanPlugins() {
  // Use the global scanner instance (created on startup)
  if (!g_pluginScanner) {
    g_pluginScanner = new MagdaPluginScanner();
    // Try to load plugins from cache first
    g_pluginScanner->LoadFromCache();
  }

  // If plugins are already scanned, just generate aliases
  if (!g_pluginScanner->GetPlugins().empty()) {
    // Plugins already exist, just generate aliases
    g_pluginScanner->GenerateAliases();
    RefreshAliasList();

    // Update status
    HWND statusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
    if (statusLabel) {
      char msg[256];
      snprintf(msg, sizeof(msg), "Aliases generated for %d plugins",
               (int)g_pluginScanner->GetPlugins().size());
      SetWindowText(statusLabel, msg);
    }
    return;
  }

  // Update status
  HWND statusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
  if (statusLabel) {
    SetWindowText(statusLabel, "Scanning plugins... (this may take a while)");
  }

  // Disable scan button during operation
  if (m_hwndScanButton) {
    EnableWindow(m_hwndScanButton, FALSE);
  }

  // Start async scan
  g_pluginScanner->ScanAndGenerateAliasesAsync(ScanAsyncCallback);
}

void MagdaPluginWindow::RefreshAliasList() {
  if (!m_hwndAliasList) {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: RefreshAliasList: m_hwndAliasList is NULL!\n");
      }
    }
    return;
  }

  // Clear list view
  ListView_DeleteAllItems(m_hwndAliasList);

  // Log that we're clearing
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: RefreshAliasList: Cleared ListView, starting to "
                     "populate...\n");
    }
  }

  // Ensure scanner exists - use global instance
  if (!g_pluginScanner) {
    g_pluginScanner = new MagdaPluginScanner();
    // Try to load plugins from cache (aliases are loaded in constructor)
    g_pluginScanner->LoadFromCache();
  }

  // Get all plugins (not just those with aliases)
  const auto &plugins = g_pluginScanner->GetPlugins();

  // Debug: log plugin count
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: RefreshAliasList: %d plugins, %d aliases\n",
               (int)plugins.size(),
               (int)g_pluginScanner->GetAliasesByPlugin().size());
      ShowConsoleMsg(msg);
    }
  }

  if (plugins.empty()) {
    // Add a single row with message
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: No plugins found, adding empty message row\n");
      }
    }
    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = (char *)"No plugins found. Click 'Scan Plugins' to scan "
                          "installed plugins.";
    int itemIndex = ListView_InsertItem(m_hwndAliasList, &lvi);
    ListView_SetItemText(m_hwndAliasList, itemIndex, 1, (char *)"");
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "MAGDA: Inserted empty message, ListView_GetItemCount=%d\n",
                 ListView_GetItemCount(m_hwndAliasList));
        ShowConsoleMsg(msg);
      }
    }
    return;
  }

  // Get aliases (reverse mapping: full_name -> [aliases])
  const auto &aliasesByPlugin = g_pluginScanner->GetAliasesByPlugin();

  // Display all plugins with their aliases in two columns
  int row = 0;
  for (const auto &plugin : plugins) {
    // Insert row with plugin name in column 0 (not full_name, just the name)
    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = row;
    lvi.iSubItem = 0;
    // Use plugin.name instead of plugin.full_name for cleaner display
    const char *displayName =
        plugin.name.empty() ? plugin.full_name.c_str() : plugin.name.c_str();
    lvi.pszText = (char *)displayName;
    int itemIndex = ListView_InsertItem(m_hwndAliasList, &lvi);

    // Set single alias in column 1 (use shortest alias for autocomplete, filter
    // out x64)
    std::string aliasStr;
    auto it = aliasesByPlugin.find(plugin.full_name);
    if (it != aliasesByPlugin.end() && !it->second.empty()) {
      // Find the shortest alias that doesn't contain "(x64)" or similar
      const std::string *shortest = nullptr;
      size_t shortest_len = SIZE_MAX;

      for (size_t i = 0; i < it->second.size(); i++) {
        const std::string &alias = it->second[i];
        std::string alias_lower;
        for (char c : alias) {
          alias_lower += (char)::tolower(c);
        }
        // Skip aliases with bitness markers
        if (alias_lower.find("(x64)") != std::string::npos ||
            alias_lower.find("(x86)") != std::string::npos ||
            alias_lower.find("(64bit)") != std::string::npos ||
            alias_lower.find("(32bit)") != std::string::npos ||
            alias_lower.find("(64-bit)") != std::string::npos ||
            alias_lower.find("(32-bit)") != std::string::npos) {
          continue;
        }
        if (alias.length() < shortest_len) {
          shortest = &alias;
          shortest_len = alias.length();
        }
      }

      if (shortest) {
        aliasStr = *shortest;
      } else if (!it->second.empty()) {
        // Fallback: use first alias and strip any bitness markers
        aliasStr = it->second[0];
        // Remove "(x64)", "(x86)", etc. from the alias string
        size_t pos;
        while ((pos = aliasStr.find("(x64)")) != std::string::npos) {
          aliasStr.erase(pos, 5);
        }
        while ((pos = aliasStr.find("(x86)")) != std::string::npos) {
          aliasStr.erase(pos, 5);
        }
        while ((pos = aliasStr.find("(64bit)")) != std::string::npos) {
          aliasStr.erase(pos, 7);
        }
        while ((pos = aliasStr.find("(32bit)")) != std::string::npos) {
          aliasStr.erase(pos, 7);
        }
        while ((pos = aliasStr.find("(64-bit)")) != std::string::npos) {
          aliasStr.erase(pos, 8);
        }
        while ((pos = aliasStr.find("(32-bit)")) != std::string::npos) {
          aliasStr.erase(pos, 8);
        }
        // Trim any extra spaces
        while (!aliasStr.empty() && aliasStr.back() == ' ') {
          aliasStr.pop_back();
        }
        while (!aliasStr.empty() && aliasStr.front() == ' ') {
          aliasStr.erase(0, 1);
        }
      }
    } else {
      aliasStr = "(no alias)";
    }

    // Use ListView_SetItemText for subitem (single alias)
    ListView_SetItemText(m_hwndAliasList, itemIndex, 1,
                         (char *)aliasStr.c_str());

    row++;
  }

  // Force ListView to redraw after adding items
  if (row > 0) {
    InvalidateRect(m_hwndAliasList, NULL, TRUE);
    UpdateWindow(m_hwndAliasList);
  }

  // Debug: log how many items were added
  if (g_rec && row > 0) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "MAGDA: Added %d rows to listview, ListView_GetItemCount=%d\n",
               row, ListView_GetItemCount(m_hwndAliasList));
      ShowConsoleMsg(msg);
    }
  }
}
