#include "magda_chat_window.h"
#include "magda_actions.h"
#include "magda_api_client.h"
#include "magda_auth.h"
#include "magda_chat_resource.h"
#include "magda_login_window.h"
#include <cstring>

#ifndef _WIN32
#include <WDL/wdltypes.h> // Defines GWLP_USERDATA for macOS
#endif

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

// Control IDs
#define IDC_QUESTION_INPUT 1001
#define IDC_QUESTION_DISPLAY 1002
#define IDC_REPLY_DISPLAY 1003
#define IDC_SEND_BUTTON 1004

MagdaChatWindow::MagdaChatWindow()
    : m_hwnd(nullptr), m_hwndQuestionInput(nullptr), m_hwndQuestionDisplay(nullptr),
      m_hwndReplyDisplay(nullptr), m_hwndSendButton(nullptr) {}

MagdaChatWindow::~MagdaChatWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

void MagdaChatWindow::Show(bool toggle) {
  if (!g_rec)
    return;

  if (m_hwnd && IsWindowVisible(m_hwnd)) {
    if (toggle) {
      Hide();
    } else {
      SetForegroundWindow(m_hwnd);
    }
    return;
  }

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
        void (*DockWindowAddEx)(HWND hwnd, const char *name, const char *identstr, bool allowShow) =
            (void (*)(HWND, const char *, const char *, bool))g_rec->GetFunc("DockWindowAddEx");
        if (DockWindowAddEx) {
          // allowShow=false: Don't auto-show if docked, let user control visibility
          DockWindowAddEx(m_hwnd, "MAGDA Chat", "MAGDA_CHAT_WINDOW", false);

          // Refresh the dock system
          void (*DockWindowRefresh)() = (void (*)())g_rec->GetFunc("DockWindowRefresh");
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
      void (*DockWindowActivate)(HWND hwnd) = (void (*)(HWND))g_rec->GetFunc("DockWindowActivate");
      if (DockWindowActivate) {
        DockWindowActivate(m_hwnd);
      }
    } else {
      // Window is floating - show it normally
      ShowWindow(m_hwnd, SW_SHOW);
      SetForegroundWindow(m_hwnd);
      SetFocus(m_hwnd);
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
INT_PTR WINAPI MagdaChatWindow::sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  // Get 'this' pointer from GWLP_USERDATA (SWS pattern)
  MagdaChatWindow *pObj = (MagdaChatWindow *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
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

    // Validate all controls were created
    if (!m_hwndQuestionDisplay || !m_hwndReplyDisplay || !m_hwndQuestionInput ||
        !m_hwndSendButton) {
      return FALSE;
    }

    // Get window size and update layout
    RECT r;
    GetClientRect(m_hwnd, &r);
    UpdateLayout(r.right - r.left, r.bottom - r.top);

    // Add welcome messages
    AddQuestion("Welcome! Type your questions here.\n\n");
    AddReply("MAGDA: Ready to help! Your responses will appear here.\n\n");

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
      int cmd = TrackPopupMenu(hMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0,
                               m_hwnd, NULL);
      DestroyMenu(hMenu);

      if (cmd == 1000) {
        // Undock: Remove from dock system and show as floating
        if (g_rec) {
          void (*DockWindowRemove)(HWND hwnd) = (void (*)(HWND))g_rec->GetFunc("DockWindowRemove");
          if (DockWindowRemove) {
            // Remove from dock first
            DockWindowRemove(m_hwnd);

            // Refresh dock system
            void (*DockWindowRefresh)() = (void (*)())g_rec->GetFunc("DockWindowRefresh");
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
          void (*DockWindowAddEx)(HWND hwnd, const char *name, const char *identstr,
                                  bool allowShow) =
              (void (*)(HWND, const char *, const char *, bool))g_rec->GetFunc("DockWindowAddEx");
          if (DockWindowAddEx) {
            // Add back to dock system
            DockWindowAddEx(m_hwnd, "MAGDA Chat", "MAGDA_CHAT_WINDOW", true);

            // Refresh dock system
            void (*DockWindowRefresh)() = (void (*)())g_rec->GetFunc("DockWindowRefresh");
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
  }
}

void MagdaChatWindow::OnSendMessage() {
  if (!m_hwndQuestionInput)
    return;

  char buffer[1024];
  buffer[0] = '\0';
  GetWindowText(m_hwndQuestionInput, buffer, sizeof(buffer));

  if (strlen(buffer) > 0) {
    // Add question to question pane
    if (m_hwndQuestionDisplay) {
      AddQuestion(buffer);
      AddQuestion("\n");
    }

    // Clear input
    if (m_hwndQuestionInput) {
      SetWindowText(m_hwndQuestionInput, "");
    }

    // Show "Thinking..." message
    if (m_hwndReplyDisplay) {
      AddReply("MAGDA: Thinking...\n");
    }

    // Call backend API with streaming
    static MagdaHTTPClient httpClient;

    // Set JWT token if available
    const char *token = MagdaLoginWindow::GetStoredToken();
    if (token && token[0]) {
      httpClient.SetJWTToken(token);

      // Log token retrieval for debugging
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char log_msg[256];
          snprintf(log_msg, sizeof(log_msg),
                   "MAGDA: Retrieved JWT token (length: %d) from storage\n", (int)strlen(token));
          ShowConsoleMsg(log_msg);
        }
      }
    } else {
      // Log missing token
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: WARNING - No JWT token found in storage\n");
        }
      }
    }

    WDL_FastString error_msg;
    struct StreamContext {
      MagdaChatWindow *window;
      int action_count;
    };
    static StreamContext s_ctx;
    s_ctx.window = this;
    s_ctx.action_count = 0;

    // Stream callback - executes each action as it arrives
    // Must be a plain function pointer (no lambda captures)
    static auto stream_callback = [](const char *action_json, void *user_data) -> void {
      StreamContext *ctx = (StreamContext *)user_data;
      ctx->action_count++;

      // Execute the action immediately
      WDL_FastString result, action_error;
      if (MagdaActions::ExecuteActions(action_json, result, action_error)) {
        if (ctx->window->m_hwndReplyDisplay) {
          char msg[256];
          snprintf(msg, sizeof(msg), "MAGDA: Action #%d executed\n", ctx->action_count);
          ctx->window->AddReply(msg);
        }
      } else {
        if (ctx->window->m_hwndReplyDisplay) {
          char msg[512];
          snprintf(msg, sizeof(msg), "MAGDA: Action #%d failed: %s\n", ctx->action_count,
                   action_error.Get());
          ctx->window->AddReply(msg);
        }
      }
    };

    if (httpClient.SendQuestionStream(buffer, stream_callback, &s_ctx, error_msg)) {
      if (m_hwndReplyDisplay) {
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA: Stream complete (%d actions)\n\n", s_ctx.action_count);
        AddReply(msg);
      }
    } else {
      // Check if error is 401 (unauthorized) - try to refresh token
      if (strstr(error_msg.Get(), "401") || strstr(error_msg.Get(), "Unauthorized")) {
        if (g_rec) {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA: Token expired, attempting refresh...\n");
          }
        }

        WDL_FastString refresh_error;
        if (MagdaAuth::RefreshToken(refresh_error)) {
          // Refresh succeeded - retry the request with new token
          const char *new_token = MagdaLoginWindow::GetStoredToken();
          if (new_token && new_token[0]) {
            httpClient.SetJWTToken(new_token);
            if (g_rec) {
              void (*ShowConsoleMsg)(const char *msg) =
                  (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
              if (ShowConsoleMsg) {
                ShowConsoleMsg("MAGDA: Token refreshed, retrying request...\n");
              }
            }

            // Reset action count and retry
            s_ctx.action_count = 0;
            if (httpClient.SendQuestionStream(buffer, stream_callback, &s_ctx, error_msg)) {
              if (m_hwndReplyDisplay) {
                char msg[256];
                snprintf(msg, sizeof(msg), "MAGDA: Stream complete (%d actions)\n\n",
                         s_ctx.action_count);
                AddReply(msg);
              }
              return; // Success after refresh
            }
          }
        } else {
          // Refresh failed - user needs to re-login
          if (m_hwndReplyDisplay) {
            AddReply("MAGDA: Session expired. Please log in again.\n\n");
          }
          if (g_rec) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              ShowConsoleMsg("MAGDA: Token refresh failed - user must re-login\n");
            }
          }
          return;
        }
      }

      // Other errors or refresh retry failed
      if (m_hwndReplyDisplay) {
        char response[2048];
        snprintf(response, sizeof(response), "MAGDA: Error: %s\n\n", error_msg.Get());
        AddReply(response);
      }
    }
  }
}

void MagdaChatWindow::AddQuestion(const char *question) {
  if (!m_hwndQuestionDisplay || !question)
    return;

  int len = GetWindowTextLength(m_hwndQuestionDisplay);
  SendMessage(m_hwndQuestionDisplay, EM_SETSEL, len, len);
  SendMessage(m_hwndQuestionDisplay, EM_REPLACESEL, FALSE, (LPARAM)question);
  int newLen = GetWindowTextLength(m_hwndQuestionDisplay);
  SendMessage(m_hwndQuestionDisplay, EM_SETSEL, newLen, newLen);
}

void MagdaChatWindow::AddReply(const char *reply) {
  if (!m_hwndReplyDisplay || !reply)
    return;

  int len = GetWindowTextLength(m_hwndReplyDisplay);
  SendMessage(m_hwndReplyDisplay, EM_SETSEL, len, len);
  SendMessage(m_hwndReplyDisplay, EM_REPLACESEL, FALSE, (LPARAM)reply);
  int newLen = GetWindowTextLength(m_hwndReplyDisplay);
  SendMessage(m_hwndReplyDisplay, EM_SETSEL, newLen, newLen);
}

void MagdaChatWindow::UpdateLayout(int width, int height) {
  if (!m_hwnd)
    return;

  // Update layout when window is resized
  if (width < 200)
    width = 200;
  if (height < 100)
    height = 100;

  int padding = 10;
  int inputHeight = 30;
  int buttonWidth = 70;
  int buttonHeight = 30;
  int spacing = 10;

  // Calculate available height for display panes
  int displayHeight = height - padding * 3 - inputHeight;
  if (displayHeight < 50)
    displayHeight = 50;

  // Calculate width for each display pane
  int paneWidth = (width - padding * 3 - spacing) / 2;
  if (paneWidth < 100)
    paneWidth = 100;

  // Question display: left pane
  if (m_hwndQuestionDisplay) {
    SetWindowPos(m_hwndQuestionDisplay, NULL, padding, padding, paneWidth, displayHeight,
                 SWP_NOZORDER);
  }

  // Reply display: right pane
  if (m_hwndReplyDisplay) {
    SetWindowPos(m_hwndReplyDisplay, NULL, padding + paneWidth + spacing, padding, paneWidth,
                 displayHeight, SWP_NOZORDER);
  }

  // Input field: bottom
  if (m_hwndQuestionInput) {
    int inputY = height - padding - inputHeight;
    int inputWidth = width - padding * 2 - buttonWidth - spacing;
    if (inputWidth < 50)
      inputWidth = 50;
    SetWindowPos(m_hwndQuestionInput, NULL, padding, inputY, inputWidth, inputHeight, SWP_NOZORDER);
  }

  // Send button: bottom right
  if (m_hwndSendButton) {
    int buttonY = height - padding - buttonHeight;
    SetWindowPos(m_hwndSendButton, NULL, width - padding - buttonWidth, buttonY, buttonWidth,
                 buttonHeight, SWP_NOZORDER);
  }
}
