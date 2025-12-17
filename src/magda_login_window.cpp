#include "magda_login_window.h"
#include "magda_auth.h"
#include "magda_env.h"
#include "magda_login_resource.h"
#include <cstdio>
#include <cstring>

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

// Global static variable to hold the window handle for the async callback
static HWND g_loginWindowHwnd = nullptr;

// Data structure for login completion message
struct LoginCompleteData {
  bool success;
  WDL_FastString jwt_token;
  WDL_FastString error_msg;
};

// Static callback wrapper - converts function pointer callback to PostMessage
static void LoginCallbackWrapper(HWND hwnd, bool success, const char *token,
                                 const char *error) {
  if (hwnd) {
    LoginCompleteData *data = new LoginCompleteData();
    data->success = success;
    if (token) {
      data->jwt_token.Set(token);
    }
    if (error) {
      data->error_msg.Set(error);
    }
    // Post message to dialog window - will be handled on main thread
    PostMessage(hwnd, WM_LOGIN_COMPLETE, 0, (LPARAM)data);
  }
}

// Static callback function for LoginAsync - must be a plain C function pointer
// This is called from the background thread, so it uses PostMessage to
// communicate with the main thread safely
static void LoginAsyncCallback(bool success, const char *token,
                               const char *error) {
  // Use static window handle set before starting login
  LoginCallbackWrapper(g_loginWindowHwnd, success, token, error);
}

// Static storage for JWT token (function-local static to avoid initialization
// order issues)
WDL_FastString &MagdaLoginWindow::GetTokenStorage() {
  static WDL_FastString s_jwt_token;
  return s_jwt_token;
}

MagdaLoginWindow::MagdaLoginWindow()
    : m_hwnd(nullptr), m_hwndEmailInput(nullptr), m_hwndPasswordInput(nullptr),
      m_hwndLoginButton(nullptr), m_hwndStatusLabel(nullptr),
      m_hwndStatusIcon(nullptr) {}

MagdaLoginWindow::~MagdaLoginWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

const char *MagdaLoginWindow::GetStoredToken() {
  // Use MagdaAuth's persistent storage
  return MagdaAuth::GetStoredToken();
}

void MagdaLoginWindow::StoreToken(const char *token) {
  // Use MagdaAuth's persistent storage
  MagdaAuth::StoreToken(token);
}

// Helper functions to store/load credentials persistently
static void StoreCredentials(const char *email, const char *password) {
  if (!g_rec)
    return;
  void (*SetExtState)(const char *section, const char *key, const char *value,
                      bool persist) =
      (void (*)(const char *, const char *, const char *, bool))g_rec->GetFunc(
          "SetExtState");
  if (SetExtState) {
    SetExtState("MAGDA", "email", email ? email : "", true);
    SetExtState("MAGDA", "password", password ? password : "", true);
  }
}

static void LoadCredentials(WDL_FastString &email, WDL_FastString &password) {
  if (!g_rec)
    return;
  const char *(*GetExtState)(const char *section, const char *key) =
      (const char *(*)(const char *, const char *))g_rec->GetFunc(
          "GetExtState");
  if (GetExtState) {
    const char *stored_email = GetExtState("MAGDA", "email");
    const char *stored_password = GetExtState("MAGDA", "password");
    if (stored_email && strlen(stored_email) > 0) {
      email.Set(stored_email);
    }
    if (stored_password && strlen(stored_password) > 0) {
      password.Set(stored_password);
    }
  }
}

void MagdaLoginWindow::Show(bool toggle) {
  if (!g_rec || !g_hInst)
    return;

  if (m_hwnd && IsWindowVisible(m_hwnd)) {
    if (toggle) {
      Hide();
    } else {
      SetForegroundWindow(m_hwnd);
      SetFocus(m_hwndEmailInput);
    }
    return;
  }

  if (!m_hwnd) {
    // Create a modeless dialog - SWS pattern
    m_hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MAGDA_LOGIN), NULL,
                               sDialogProc, (LPARAM)this);
  }

  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);

    // Ensure controls are initialized
    if (!m_hwndEmailInput || !m_hwndPasswordInput || !m_hwndLoginButton) {
      m_hwndEmailInput = GetDlgItem(m_hwnd, IDC_EMAIL_INPUT);
      m_hwndPasswordInput = GetDlgItem(m_hwnd, IDC_PASSWORD_INPUT);
      m_hwndLoginButton = GetDlgItem(m_hwnd, IDC_LOGIN_BUTTON);
      m_hwndStatusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
      m_hwndStatusIcon = GetDlgItem(m_hwnd, IDC_STATUS_ICON);
    }

    SetFocus(m_hwndEmailInput);
    // Clear previous status
    SetStatus("", false);

    // Log credentials from .env when window opens (dev utility)
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg), "MAGDA Login (.env) - Email: %s\n",
               MagdaEnv::Get("MAGDA_EMAIL", ""));
      ShowConsoleMsg(log_msg);
      snprintf(log_msg, sizeof(log_msg), "MAGDA Login (.env) - Password: %s\n",
               MagdaEnv::Get("MAGDA_PASSWORD", ""));
      ShowConsoleMsg(log_msg);
    }

    // If dialog already exists, refresh UI state based on login status
    if (m_hwndEmailInput && m_hwndPasswordInput && m_hwndLoginButton) {
      // Load and populate saved credentials if available
      WDL_FastString stored_email, stored_password;
      LoadCredentials(stored_email, stored_password);
      if (stored_email.GetLength() > 0) {
        SetWindowText(m_hwndEmailInput, stored_email.Get());
      }
      if (stored_password.GetLength() > 0) {
        SetWindowText(m_hwndPasswordInput, stored_password.Get());
      }

      // Check if already logged in (have valid token) and set UI state
      // accordingly
      const char *token = GetStoredToken();
      if (token && strlen(token) > 0) {
        UpdateUIForLoggedInState();
      } else {
        UpdateUIForLoggedOutState();
      }
    }
  }
}

void MagdaLoginWindow::Hide() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

INT_PTR WINAPI MagdaLoginWindow::sDialogProc(HWND hwnd, UINT uMsg,
                                             WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_INITDIALOG) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
  }
  MagdaLoginWindow *pThis =
      (MagdaLoginWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (pThis) {
    pThis->m_hwnd = hwnd; // Store HWND in instance
    return pThis->DialogProc(uMsg, wParam, lParam);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR MagdaLoginWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG: {
    // Get control handles
    m_hwndEmailInput = GetDlgItem(m_hwnd, IDC_EMAIL_INPUT);
    m_hwndPasswordInput = GetDlgItem(m_hwnd, IDC_PASSWORD_INPUT);
    m_hwndLoginButton = GetDlgItem(m_hwnd, IDC_LOGIN_BUTTON);
    m_hwndStatusLabel = GetDlgItem(m_hwnd, IDC_STATUS_LABEL);
    m_hwndStatusIcon = GetDlgItem(m_hwnd, IDC_STATUS_ICON);

    // Validate all controls were created
    if (!m_hwndEmailInput || !m_hwndPasswordInput || !m_hwndLoginButton ||
        !m_hwndStatusLabel || !m_hwndStatusIcon) {
      return FALSE;
    }

    // Set initial focus to email field
    SetFocus(m_hwndEmailInput);
    // Clear status
    SetStatus("", false);

    // Load and populate saved credentials if available
    WDL_FastString stored_email, stored_password;
    LoadCredentials(stored_email, stored_password);
    if (stored_email.GetLength() > 0) {
      SetWindowText(m_hwndEmailInput, stored_email.Get());
    }
    if (stored_password.GetLength() > 0) {
      SetWindowText(m_hwndPasswordInput, stored_password.Get());
    }

    // Check if already logged in (have valid token) and set UI state
    // accordingly Only disable fields if we're actually logged in
    const char *token = GetStoredToken();
    if (token && strlen(token) > 0) {
      UpdateUIForLoggedInState();
    } else {
      UpdateUIForLoggedOutState();
    }

    return TRUE;
  }
  case WM_COMMAND:
    OnCommand(LOWORD(wParam), HIWORD(wParam));
    return 0;
  case WM_CLOSE:
    Hide();
    return 0;
  case WM_LOGIN_COMPLETE: {
    LoginCompleteData *data = (LoginCompleteData *)lParam;
    if (data) {
      OnLoginComplete(data->success, data->jwt_token.Get(),
                      data->error_msg.Get());
      delete data; // Free the allocated data
    }
    return 0;
  }
  }
  return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MagdaLoginWindow::OnCommand(int command, int notifyCode) {
  switch (command) {
  case IDC_LOGIN_BUTTON:
    OnLogin();
    break;
  case IDC_CANCEL_BUTTON:
    Hide();
    break;
  }
}

void MagdaLoginWindow::OnLogin() {
  if (!m_hwndEmailInput || !m_hwndPasswordInput) {
    return;
  }

  // Check if already logged in - if so, logout
  const char *token = GetStoredToken();
  if (token && strlen(token) > 0) {
    // Logout - clear token and credentials
    StoreToken(nullptr);
    if (g_rec) {
      void (*SetExtState)(const char *section, const char *key,
                          const char *value, bool persist) =
          (void (*)(const char *, const char *, const char *,
                    bool))g_rec->GetFunc("SetExtState");
      if (SetExtState) {
        SetExtState("MAGDA", "email", "", true);
        SetExtState("MAGDA", "password", "", true);
      }
    }
    UpdateUIForLoggedOutState();
    SetStatus("Logged out", false);
    return;
  }

  // Try to load stored credentials first
  WDL_FastString stored_email, stored_password;
  LoadCredentials(stored_email, stored_password);

  // Fallback to .env if no stored credentials
  const char *email = nullptr;
  const char *password = nullptr;
  if (stored_email.GetLength() > 0 && stored_password.GetLength() > 0) {
    email = stored_email.Get();
    password = stored_password.Get();
  } else {
    // Read credentials from .env for development
    email = MagdaEnv::Get("MAGDA_EMAIL", "");
    password = MagdaEnv::Get("MAGDA_PASSWORD", "");
  }

  if (!email || strlen(email) == 0 || !password || strlen(password) == 0) {
    SetStatus("Please ensure MAGDA_EMAIL and MAGDA_PASSWORD are set in .env",
              true);
    return;
  }

  OnLoginWithCredentials(email, password);
}

void MagdaLoginWindow::OnLoginWithCredentials(const char *email,
                                              const char *password) {
  if (!email || !password || strlen(email) == 0 || strlen(password) == 0) {
    SetStatus("Email and password required", true);
    return;
  }

  // Disable all controls during login
  EnableWindow(m_hwndEmailInput, FALSE);
  EnableWindow(m_hwndPasswordInput, FALSE);
  EnableWindow(m_hwndLoginButton, FALSE);

  // Show "Logging in..." status
  SetStatus("Logging in...", false);

  // Store window handle in static variable for callback
  g_loginWindowHwnd = m_hwnd;

  // Start login in background thread using MagdaAuth service
  // Use static function pointer instead of lambda for thread safety
  MagdaAuth::LoginAsync(email, password, LoginAsyncCallback);
}

void MagdaLoginWindow::OnLoginComplete(bool success, const char *token,
                                       const char *error) {
  if (success) {
    StoreToken(token);

    // Store credentials persistently for next time
    WDL_FastString stored_email, stored_password;
    LoadCredentials(stored_email, stored_password);
    if (stored_email.GetLength() == 0 || stored_password.GetLength() == 0) {
      // Store from .env if not already stored
      const char *email = MagdaEnv::Get("MAGDA_EMAIL", "");
      const char *password = MagdaEnv::Get("MAGDA_PASSWORD", "");
      if (strlen(email) > 0 && strlen(password) > 0) {
        StoreCredentials(email, password);
      }
    }

    SetStatus("Login successful!", false);
    UpdateUIForLoggedInState();
  } else {
    SetStatus(error ? error : "Login failed", true);
    UpdateUIForLoggedOutState();
  }
}

void MagdaLoginWindow::SetStatus(const char *status, bool isError) {
  if (m_hwndStatusLabel) {
    SetWindowText(m_hwndStatusLabel, status ? status : "");
  }
  // Set icon only if there's a status message
  if (m_hwndStatusIcon) {
    if (status && strlen(status) > 0) {
      SetWindowText(m_hwndStatusIcon,
                    isError ? "❌" : "✅"); // Unicode characters for status
    } else {
      SetWindowText(m_hwndStatusIcon, ""); // Clear icon if no status
    }
  }
}

void MagdaLoginWindow::UpdateUIForLoggedInState() {
  if (m_hwndEmailInput)
    EnableWindow(m_hwndEmailInput, FALSE);
  if (m_hwndPasswordInput)
    EnableWindow(m_hwndPasswordInput, FALSE);
  if (m_hwndLoginButton) {
    SetWindowText(m_hwndLoginButton, "Logout");
    EnableWindow(m_hwndLoginButton, TRUE); // Enable logout button
  }
}

void MagdaLoginWindow::UpdateUIForLoggedOutState() {
  if (m_hwndEmailInput)
    EnableWindow(m_hwndEmailInput, TRUE);
  if (m_hwndPasswordInput)
    EnableWindow(m_hwndPasswordInput, TRUE);
  if (m_hwndLoginButton)
    SetWindowText(m_hwndLoginButton, "Login");
  SetStatus("", false); // Clear status on logout
}
