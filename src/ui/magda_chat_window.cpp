#include "magda_chat_window.h"
#include "magda_actions.h"
#include "magda_api_client.h"
#include "magda_auth.h"
#include "magda_chat_resource.h"
#include "magda_env.h"
#include "magda_imgui_login.h"
#include <cstring>

#ifndef _WIN32
#include <WDL/wdltypes.h> // Defines GWLP_USERDATA for macOS
#endif

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

// Forward declaration
extern void magdaAction(int command_id, int flag);

// Command ID for mix analysis (defined in main.cpp, dynamically allocated)
extern int g_cmdMixAnalyze;

// Control IDs
#define IDC_QUESTION_INPUT 1001
#define IDC_QUESTION_DISPLAY 1002
#define IDC_REPLY_DISPLAY 1003
#define IDC_SEND_BUTTON 1004
#define IDC_REQUEST_HEADER 1005
#define IDC_RESPONSE_HEADER 1006
#define IDC_STATUS_FOOTER 1007
#define IDC_CONTROLS_HEADER 1008
#define IDC_BTN_MIX_ANALYSIS 1010
#define IDC_BTN_MASTER_ANALYSIS 1011
#define IDC_BTN_GAIN_STAGING 1012
#define IDC_BTN_HOUSEKEEPING 1013

MagdaChatWindow::MagdaChatWindow()
    : m_hwnd(nullptr), m_hwndQuestionInput(nullptr),
      m_hwndQuestionDisplay(nullptr), m_hwndReplyDisplay(nullptr),
      m_hwndSendButton(nullptr), m_hwndRequestHeader(nullptr),
      m_hwndResponseHeader(nullptr), m_hwndControlsHeader(nullptr),
      m_hwndStatusFooter(nullptr), m_requestLineCount(0),
      m_responseLineCount(0) {}

MagdaChatWindow::~MagdaChatWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

void MagdaChatWindow::Show(bool toggle) {
  if (!g_rec)
    return;

  // Check if window exists and is valid
  bool windowExists = (m_hwnd != nullptr);
  bool windowVisible = false;
  if (windowExists) {
    // Check if window handle is still valid (not destroyed)
    // On some systems, IsWindow can be unreliable, so we also check if it's
    // visible
    windowVisible = IsWindowVisible(m_hwnd);

    if (windowVisible) {
      if (toggle) {
        Hide();
      } else {
        // Window is visible - just bring to front
        SetForegroundWindow(m_hwnd);
      }
      return;
    }
  }

  // Window doesn't exist or is hidden - create or show it
  if (!m_hwnd) {
    // Create a modeless dialog - SWS pattern
    CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MAGDA_CHAT),
                      NULL, // NULL parent = top-level floating window
                      sDialogProc, (LPARAM)this);

    // Show window first as floating, then add to dock system
    // This ensures it can be properly undocked
    if (m_hwnd) {
      ShowWindow(m_hwnd, SW_SHOW);

      // Add window to REAPER's dock system AFTER showing
      // This makes it dockable while keeping it undockable
      if (g_rec) {
        void (*DockWindowAddEx)(HWND hwnd, const char *name,
                                const char *identstr, bool allowShow) =
            (void (*)(HWND, const char *, const char *, bool))g_rec->GetFunc(
                "DockWindowAddEx");
        if (DockWindowAddEx) {
          // allowShow=false: Don't auto-show if docked, let user control
          // visibility
          DockWindowAddEx(m_hwnd, "MAGDA Chat", "MAGDA_CHAT_WINDOW", false);

          // Refresh the dock system
          void (*DockWindowRefresh)() =
              (void (*)())g_rec->GetFunc("DockWindowRefresh");
          if (DockWindowRefresh) {
            DockWindowRefresh();
          }
        }
      }
    }
  }

  if (m_hwnd) {
    // Check if window is docked
    bool isDocked = false;
    if (g_rec) {
      int (*DockIsChildOfDock)(HWND hwnd, bool *isFloatingDockerOut) =
          (int (*)(HWND, bool *))g_rec->GetFunc("DockIsChildOfDock");
      if (DockIsChildOfDock) {
        bool isFloating = false;
        int dockIndex = DockIsChildOfDock(m_hwnd, &isFloating);
        isDocked = (dockIndex >= 0);
      }
    }

    if (isDocked) {
      // Window is docked - activate the dock tab
      // This works even if the window was previously hidden/closed
      void (*DockWindowActivate)(HWND hwnd) =
          (void (*)(HWND))g_rec->GetFunc("DockWindowActivate");
      if (DockWindowActivate) {
        DockWindowActivate(m_hwnd);
      }
    } else {
      // Window is floating - show it normally
      // Always show, even if it was previously hidden
      ShowWindow(m_hwnd, SW_SHOW);
      SetForegroundWindow(m_hwnd);
      SetFocus(m_hwnd);
      UpdateWindow(m_hwnd); // Ensure window is updated
    }
  }
}

void MagdaChatWindow::Hide() {
  if (m_hwnd) {
    // Check if window is docked
    bool isDocked = false;
    if (g_rec) {
      int (*DockIsChildOfDock)(HWND hwnd, bool *isFloatingDockerOut) =
          (int (*)(HWND, bool *))g_rec->GetFunc("DockIsChildOfDock");
      if (DockIsChildOfDock) {
        bool isFloating = false;
        int dockIndex = DockIsChildOfDock(m_hwnd, &isFloating);
        isDocked = (dockIndex >= 0);
      }
    }

    if (isDocked) {
      // For docked windows, REAPER manages visibility
      // We can't really "hide" a docked window, but we can deactivate it
      // The user can close the dock tab manually
    } else {
      // Floating window - hide normally
      ShowWindow(m_hwnd, SW_HIDE);
    }
  }
}

// Static dialog proc - SWS pattern
INT_PTR WINAPI MagdaChatWindow::sDialogProc(HWND hwndDlg, UINT uMsg,
                                            WPARAM wParam, LPARAM lParam) {
  // Get 'this' pointer from GWLP_USERDATA (SWS pattern)
  MagdaChatWindow *pObj =
      (MagdaChatWindow *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
  if (!pObj && uMsg == WM_INITDIALOG) {
    // Store 'this' pointer from lParam (passed from CreateDialogParam)
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
    pObj = (MagdaChatWindow *)lParam;
    pObj->m_hwnd = hwndDlg;
  }
  return pObj ? pObj->DialogProc(uMsg, wParam, lParam) : 0;
}

// Instance dialog proc - handles all messages
INT_PTR MagdaChatWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    // Get control handles
    m_hwndQuestionDisplay = GetDlgItem(m_hwnd, IDC_QUESTION_DISPLAY);
    m_hwndReplyDisplay = GetDlgItem(m_hwnd, IDC_REPLY_DISPLAY);
    m_hwndQuestionInput = GetDlgItem(m_hwnd, IDC_QUESTION_INPUT);
    m_hwndSendButton = GetDlgItem(m_hwnd, IDC_SEND_BUTTON);
    m_hwndRequestHeader = GetDlgItem(m_hwnd, IDC_REQUEST_HEADER);
    m_hwndResponseHeader = GetDlgItem(m_hwnd, IDC_RESPONSE_HEADER);
    m_hwndControlsHeader = GetDlgItem(m_hwnd, IDC_CONTROLS_HEADER);
    m_hwndStatusFooter = GetDlgItem(m_hwnd, IDC_STATUS_FOOTER);

    // Validate required controls were created
    if (!m_hwndQuestionDisplay || !m_hwndReplyDisplay || !m_hwndQuestionInput ||
        !m_hwndSendButton) {
      return FALSE;
    }

    // Get window size and update layout
    RECT r;
    GetClientRect(m_hwnd, &r);
    UpdateLayout(r.right - r.left, r.bottom - r.top);

    // Reset line counter
    m_requestLineCount = 0;
    m_responseLineCount = 0;

    // Clear any initial text
    if (m_hwndQuestionDisplay) {
      SetWindowText(m_hwndQuestionDisplay, "");
    }
    if (m_hwndReplyDisplay) {
      SetWindowText(m_hwndReplyDisplay, "");
    }

    // Check API health
    CheckAPIHealth();

    return TRUE;
  }

  case WM_COMMAND:
    OnCommand(LOWORD(wParam), HIWORD(wParam));
    return 0;

  case WM_SIZE: {
    RECT r;
    GetClientRect(m_hwnd, &r);
    int width = r.right - r.left;
    int height = r.bottom - r.top;
    if (width > 100 && height > 100) {
      UpdateLayout(width, height);
    }
    return 0;
  }

  case WM_CTLCOLORSTATIC: {
    // Set darker color for header labels
    HWND hCtrl = (HWND)lParam;
    if (hCtrl == m_hwndRequestHeader || hCtrl == m_hwndResponseHeader ||
        hCtrl == m_hwndControlsHeader) {
      HDC hdc = (HDC)wParam;
      SetTextColor(hdc, RGB(80, 80, 80)); // Dark gray
      SetBkMode(hdc, TRANSPARENT);
      return (INT_PTR)GetStockObject(NULL_BRUSH);
    }
    break;
  }

  case WM_CLOSE:
    Hide();
    return 0;

  case WM_CONTEXTMENU: {
    // Check if window is docked
    bool isDocked = false;
    if (g_rec) {
      int (*DockIsChildOfDock)(HWND hwnd, bool *isFloatingDockerOut) =
          (int (*)(HWND, bool *))g_rec->GetFunc("DockIsChildOfDock");
      if (DockIsChildOfDock) {
        bool isFloating = false;
        int dockIndex = DockIsChildOfDock(m_hwnd, &isFloating);
        isDocked = (dockIndex >= 0);
      }
    }

    // Show context menu with Dock/Undock option
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
      MENUITEMINFO mi = {sizeof(MENUITEMINFO)};
      mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
      mi.fType = MFT_STRING;
      mi.fState = MFS_ENABLED;

      if (isDocked) {
        // Window is docked - show "Undock" option
        mi.wID = 1000; // Undock command ID
        mi.dwTypeData = (char *)"Undock";
        InsertMenuItem(hMenu, 0, TRUE, &mi);
      } else {
        // Window is floating - show "Dock" option
        mi.wID = 1001; // Dock command ID
        mi.dwTypeData = (char *)"Dock";
        InsertMenuItem(hMenu, 0, TRUE, &mi);
      }

      POINT pt;
      GetCursorPos(&pt);
      int cmd =
          TrackPopupMenu(hMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN,
                         pt.x, pt.y, 0, m_hwnd, NULL);
      DestroyMenu(hMenu);

      if (cmd == 1000) {
        // Undock: Remove from dock system and show as floating
        if (g_rec) {
          void (*DockWindowRemove)(HWND hwnd) =
              (void (*)(HWND))g_rec->GetFunc("DockWindowRemove");
          if (DockWindowRemove) {
            // Remove from dock first
            DockWindowRemove(m_hwnd);

            // Refresh dock system
            void (*DockWindowRefresh)() =
                (void (*)())g_rec->GetFunc("DockWindowRefresh");
            if (DockWindowRefresh) {
              DockWindowRefresh();
            }

            // Ensure window has no parent (top-level window)
            SetParent(m_hwnd, NULL);

            // Get current window position and size
            RECT rect;
            GetWindowRect(m_hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            // If window has no valid size (was docked), set default size
            if (width < 100 || height < 100) {
              width = 1000;
              height = 600;
            }

            // Center window on screen if position is invalid
            if (rect.left < 0 || rect.top < 0) {
              int screenWidth = GetSystemMetrics(SM_CXSCREEN);
              int screenHeight = GetSystemMetrics(SM_CYSCREEN);
              rect.left = (screenWidth - width) / 2;
              rect.top = (screenHeight - height) / 2;
            }

            // Show as floating window with proper positioning
            // Use SWP_FRAMECHANGED to ensure proper window frame
            SetWindowPos(m_hwnd, HWND_TOP, rect.left, rect.top, width, height,
                         SWP_SHOWWINDOW | SWP_FRAMECHANGED);
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
            SetForegroundWindow(m_hwnd);
            SetFocus(m_hwnd);
          }
        }
      } else if (cmd == 1001) {
        // Dock: Add back to dock system
        if (g_rec) {
          void (*DockWindowAddEx)(HWND hwnd, const char *name,
                                  const char *identstr, bool allowShow) =
              (void (*)(HWND, const char *, const char *, bool))g_rec->GetFunc(
                  "DockWindowAddEx");
          if (DockWindowAddEx) {
            // Add back to dock system
            DockWindowAddEx(m_hwnd, "MAGDA Chat", "MAGDA_CHAT_WINDOW", true);

            // Refresh dock system
            void (*DockWindowRefresh)() =
                (void (*)())g_rec->GetFunc("DockWindowRefresh");
            if (DockWindowRefresh) {
              DockWindowRefresh();
            }

            // Activate the docked window
            void (*DockWindowActivate)(HWND hwnd) =
                (void (*)(HWND))g_rec->GetFunc("DockWindowActivate");
            if (DockWindowActivate) {
              DockWindowActivate(m_hwnd);
            }
          }
        }
      }
    }
    return 0;
  }

  case WM_DESTROY:
    m_hwnd = nullptr;
    return 0;

  default:
    return 0;
  }
}

void MagdaChatWindow::OnCommand(int command, int notifyCode) {
  switch (command) {
  case IDC_SEND_BUTTON:
    OnSendMessage();
    break;

  case IDOK: // Enter key in input field
    if (m_hwndQuestionInput && GetFocus() == m_hwndQuestionInput) {
      OnSendMessage();
    }
    break;

  case IDC_BTN_MIX_ANALYSIS:
    // Trigger mix analysis workflow (bounce/analyze/send to agent)
    magdaAction(g_cmdMixAnalyze, 0);
    break;

  case IDC_BTN_MASTER_ANALYSIS:
    // For now, send chat message (can be updated later)
    if (m_hwndQuestionInput) {
      SetWindowText(m_hwndQuestionInput,
                    "Analyze the master bus and suggest mastering adjustments");
      OnSendMessage();
    }
    break;

  case IDC_BTN_GAIN_STAGING:
    // For now, send chat message (can be updated later)
    if (m_hwndQuestionInput) {
      SetWindowText(m_hwndQuestionInput,
                    "Check gain staging across all tracks");
      OnSendMessage();
    }
    break;

  case IDC_BTN_HOUSEKEEPING:
    // For now, send chat message (can be updated later)
    if (m_hwndQuestionInput) {
      SetWindowText(m_hwndQuestionInput, "Clean up and organize this project");
      OnSendMessage();
    }
    break;
  }
}

void MagdaChatWindow::OnSendMessage() {
  if (!m_hwndQuestionInput)
    return;

  char buffer[1024];
  buffer[0] = '\0';
  GetWindowText(m_hwndQuestionInput, buffer, sizeof(buffer));

  if (strlen(buffer) > 0) {
    // Add separator if not first message
    if (m_responseLineCount > 0) {
      AddRequest("─────────────────────────────\n");
      AddResponse("─────────────────────────────\n");
    }

    // Add request to request pane
    if (m_hwndQuestionDisplay) {
      AddRequest(buffer);
      AddRequest("\n");
    }

    // Clear input
    if (m_hwndQuestionInput) {
      SetWindowText(m_hwndQuestionInput, "");
    }

    // Show "Processing..." message
    if (m_hwndReplyDisplay) {
      AddResponse("Processing...\n");
    }

    // Align columns after adding request
    AlignRequestWithResponse();

    // Call backend API with non-streaming endpoint (uses DSL/CFG)
    static MagdaHTTPClient httpClient;

    // Only set JWT token if auth is required (Gateway mode)
    // Local API (AuthMode::None) doesn't need authentication
    if (g_imguiLogin && g_imguiLogin->GetAuthMode() == AuthMode::Gateway) {
      const char *token = MagdaImGuiLogin::GetStoredToken();
      if (token && token[0]) {
        httpClient.SetJWTToken(token);
      }
    } else {
      httpClient.SetJWTToken(nullptr);
    }

    WDL_FastString response_json, error_msg;

    if (httpClient.SendQuestion(buffer, response_json, error_msg)) {
      // Actions are automatically executed by SendQuestion
      if (m_hwndReplyDisplay) {
        AddResponse("Done\n");
      }
    } else {
      // Token refresh is now handled automatically by HTTP client
      // If we still have an error, it means refresh failed or it's a different
      // error

      // Other errors or refresh retry failed
      if (m_hwndReplyDisplay) {
        char response[2048];
        snprintf(response, sizeof(response), "Error: %s\n", error_msg.Get());
        AddResponse(response);
      }
    }

    // Align after response is complete
    AlignRequestWithResponse();
  }
}

void MagdaChatWindow::AddRequest(const char *request) {
  if (!m_hwndQuestionDisplay || !request)
    return;

  int len = GetWindowTextLength(m_hwndQuestionDisplay);
  SendMessage(m_hwndQuestionDisplay, EM_SETSEL, len, len);
  SendMessage(m_hwndQuestionDisplay, EM_REPLACESEL, FALSE, (LPARAM)request);
  int newLen = GetWindowTextLength(m_hwndQuestionDisplay);
  SendMessage(m_hwndQuestionDisplay, EM_SETSEL, newLen, newLen);

  // Count newlines added for alignment tracking
  for (const char *p = request; *p; ++p) {
    if (*p == '\n') {
      m_requestLineCount++;
    }
  }
}

void MagdaChatWindow::AddResponse(const char *response) {
  if (!m_hwndReplyDisplay || !response)
    return;

  int len = GetWindowTextLength(m_hwndReplyDisplay);
  SendMessage(m_hwndReplyDisplay, EM_SETSEL, len, len);
  SendMessage(m_hwndReplyDisplay, EM_REPLACESEL, FALSE, (LPARAM)response);
  int newLen = GetWindowTextLength(m_hwndReplyDisplay);
  SendMessage(m_hwndReplyDisplay, EM_SETSEL, newLen, newLen);

  // Count newlines added for alignment tracking
  for (const char *p = response; *p; ++p) {
    if (*p == '\n') {
      m_responseLineCount++;
    }
  }
}

void MagdaChatWindow::AlignRequestWithResponse() {
  if (!m_hwndQuestionDisplay || !m_hwndReplyDisplay)
    return;

  // Add blank lines to request pane to match response pane
  while (m_requestLineCount < m_responseLineCount) {
    int len = GetWindowTextLength(m_hwndQuestionDisplay);
    SendMessage(m_hwndQuestionDisplay, EM_SETSEL, len, len);
    SendMessage(m_hwndQuestionDisplay, EM_REPLACESEL, FALSE, (LPARAM) "\n");
    m_requestLineCount++;
  }

  // Add blank lines to response pane to match request pane
  while (m_responseLineCount < m_requestLineCount) {
    int len = GetWindowTextLength(m_hwndReplyDisplay);
    SendMessage(m_hwndReplyDisplay, EM_SETSEL, len, len);
    SendMessage(m_hwndReplyDisplay, EM_REPLACESEL, FALSE, (LPARAM) "\n");
    m_responseLineCount++;
  }
}

void MagdaChatWindow::CheckAPIHealth() {
  // Update status to checking
  UpdateStatus("Checking API...", false);

  // Try to ping the API health endpoint
  static MagdaHTTPClient httpClient;
  WDL_FastString error_msg;

  bool success = httpClient.CheckHealth(error_msg, 5);

  if (success) {
    UpdateStatus("API: Connected", true);
  } else {
    char status[256];
    snprintf(status, sizeof(status), "API: Offline - %s", error_msg.Get());
    UpdateStatus(status, false);
  }
}

void MagdaChatWindow::UpdateStatus(const char *status, bool isOK) {
  if (!m_hwndStatusFooter)
    return;

  // Format status with indicator
  char statusText[512];
  if (isOK) {
    snprintf(statusText, sizeof(statusText), "● %s", status);
  } else {
    snprintf(statusText, sizeof(statusText), "○ %s", status);
  }

  SetWindowText(m_hwndStatusFooter, statusText);
}

void MagdaChatWindow::UpdateLayout(int width, int height) {
  if (!m_hwnd)
    return;

  // Update layout when window is resized
  if (width < 200)
    width = 200;
  if (height < 150)
    height = 150;

  int padding = 10;
  int headerHeight = 18; // Smaller headers
  int inputHeight = 30;
  int buttonWidth = 70;
  int buttonHeight = 30;
  int footerHeight = 25;
  int spacing = 10;

  // Calculate width for each display pane (3 columns)
  int controlsPanelWidth = 180; // Fixed width for controls panel
  int chatAreaWidth = width - padding * 2 - controlsPanelWidth - spacing * 2;
  int paneWidth = (chatAreaWidth - spacing) / 2;
  if (paneWidth < 80)
    paneWidth = 80;

  // Layout from top to bottom (in normal coords):
  // - Input row at top
  // - Headers below input
  // - Display panes (main area)
  // - Footer at bottom
  //
  // But SWELL flips Y, so we calculate in normal order then flip:
  // flipY = height - normalY - controlHeight

  // Normal Y positions (top = 0):
  int inputY_normal = padding; // Input at top
  int headerY_normal =
      padding + inputHeight + 5; // Headers below input (less padding)
  int displayTop_normal =
      headerY_normal + headerHeight + 2; // Minimal gap to display
  int displayHeight =
      height - displayTop_normal - padding - footerHeight - padding;
  if (displayHeight < 50)
    displayHeight = 50;
  int footerY_normal = displayTop_normal + displayHeight + padding;

  // Flip Y for macOS SWELL
  int inputY = height - inputY_normal - inputHeight;
  int headerY = height - headerY_normal - headerHeight;
  int displayTop = height - displayTop_normal - displayHeight;
  int footerY = height - footerY_normal - footerHeight;

  // Input field: at top (spans chat columns only, not controls)
  int inputWidth = chatAreaWidth - buttonWidth - spacing;
  if (inputWidth < 50)
    inputWidth = 50;
  if (m_hwndQuestionInput) {
    SetWindowPos(m_hwndQuestionInput, NULL, padding, inputY, inputWidth,
                 inputHeight, SWP_NOZORDER);
  }

  // Send button: right of input (aligned with chat area)
  if (m_hwndSendButton) {
    SetWindowPos(m_hwndSendButton, NULL, padding + inputWidth + spacing, inputY,
                 buttonWidth, buttonHeight, SWP_NOZORDER);
  }

  // Request header: left
  if (m_hwndRequestHeader) {
    SetWindowPos(m_hwndRequestHeader, NULL, padding, headerY, paneWidth,
                 headerHeight, SWP_NOZORDER);
  }

  // Response header: middle
  if (m_hwndResponseHeader) {
    SetWindowPos(m_hwndResponseHeader, NULL, padding + paneWidth + spacing,
                 headerY, paneWidth, headerHeight, SWP_NOZORDER);
  }

  // Controls header: right
  int controlsX = padding + paneWidth + spacing + paneWidth + spacing;
  if (m_hwndControlsHeader) {
    SetWindowPos(m_hwndControlsHeader, NULL, controlsX, headerY,
                 controlsPanelWidth, headerHeight, SWP_NOZORDER);
  }

  // Request display: left pane
  if (m_hwndQuestionDisplay) {
    SetWindowPos(m_hwndQuestionDisplay, NULL, padding, displayTop, paneWidth,
                 displayHeight, SWP_NOZORDER);
  }

  // Response display: middle pane
  if (m_hwndReplyDisplay) {
    SetWindowPos(m_hwndReplyDisplay, NULL, padding + paneWidth + spacing,
                 displayTop, paneWidth, displayHeight, SWP_NOZORDER);
  }

  // Control buttons: right column (need Y-flip like other controls)
  int btnWidth = controlsPanelWidth - 10;
  int btnHeight = 28;
  int btnSpacing = 8;
  int btnStartY_normal = headerY_normal + headerHeight + 10;

  // Button IDs - reduced set
  int buttonIDs[] = {IDC_BTN_MIX_ANALYSIS, IDC_BTN_MASTER_ANALYSIS,
                     IDC_BTN_GAIN_STAGING, IDC_BTN_HOUSEKEEPING};
  int numButtons = sizeof(buttonIDs) / sizeof(buttonIDs[0]);

  for (int i = 0; i < numButtons; i++) {
    HWND hBtn = GetDlgItem(m_hwnd, buttonIDs[i]);
    if (hBtn) {
      int btnY_normal = btnStartY_normal + i * (btnHeight + btnSpacing);
      int btnY = height - btnY_normal - btnHeight;
      SetWindowPos(hBtn, NULL, controlsX + 5, btnY, btnWidth, btnHeight,
                   SWP_NOZORDER);
    }
  }

  // Status footer: at bottom
  if (m_hwndStatusFooter) {
    SetWindowPos(m_hwndStatusFooter, NULL, padding, footerY,
                 width - padding * 2, footerHeight, SWP_NOZORDER);
  }
}
