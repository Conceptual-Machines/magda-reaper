#include "magda_login_window.h"
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/jnetlib/httpget.h"
#include "../WDL/WDL/jnetlib/util.h" // For JNL::open_socketlib()
#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/wdlstring.h"
#include "magda_api_client.h"
#include <cstring>

#ifndef _WIN32
#include <WDL/wdltypes.h>
#endif

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

// Control IDs
#define IDC_EMAIL_INPUT 2001
#define IDC_PASSWORD_INPUT 2002
#define IDC_LOGIN_BUTTON 2003
#define IDC_STATUS_LABEL 2004

// Static storage for JWT token (function-local static to avoid initialization order issues)
WDL_FastString &MagdaLoginWindow::GetTokenStorage() {
  static WDL_FastString s_jwt_token;
  return s_jwt_token;
}

MagdaLoginWindow::MagdaLoginWindow()
    : m_hwnd(nullptr), m_hwndEmailInput(nullptr), m_hwndPasswordInput(nullptr),
      m_hwndLoginButton(nullptr), m_hwndStatusLabel(nullptr) {}

MagdaLoginWindow::~MagdaLoginWindow() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
}

const char *MagdaLoginWindow::GetStoredToken() {
  WDL_FastString &token = GetTokenStorage();
  return token.GetLength() > 0 ? token.Get() : nullptr;
}

void MagdaLoginWindow::StoreToken(const char *token) {
  WDL_FastString &storage = GetTokenStorage();
  if (token) {
    storage.Set(token);
  } else {
    storage.Set("");
  }
}

void MagdaLoginWindow::Show(bool toggle) {
  if (!g_rec)
    return;

  // Use REAPER's GetUserInputs API for simple login dialog
  bool (*GetUserInputs)(const char *title, int num_inputs, const char *captions_csv,
                        char *retvals_csv, int retvals_csv_sz) =
      (bool (*)(const char *, int, const char *, char *, int))g_rec->GetFunc("GetUserInputs");

  if (!GetUserInputs) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: GetUserInputs API not available\n");
    }
    return;
  }

  // Prepare input fields: Email, Password
  // Note: "*Password:" will hide the password input (REAPER feature)
  // Use extrawidth=XXX to make fields wider (default is ~200px, we'll use 280px)
  char retvals[512] = {0};
  bool result = GetUserInputs("MAGDA Login", 2, "Email:,extrawidth=280,*Password:,extrawidth=280",
                              retvals, sizeof(retvals));

  if (!result) {
    // User cancelled
    return;
  }

  // Parse the comma-separated return values
  char email[256] = {0};
  char password[256] = {0};
  char *p = retvals;
  int field = 0;
  char *dest = email;

  while (*p && field < 2) {
    if (*p == ',') {
      *dest = '\0';
      field++;
      if (field == 1) {
        dest = password;
      }
      p++;
    } else {
      *dest++ = *p++;
    }
  }
  *dest = '\0';

  if (strlen(email) == 0 || strlen(password) == 0) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Email and password required\n");
    }
    return;
  }

  // Perform login with HTTP call
  OnLoginWithCredentials(email, password);
}

void MagdaLoginWindow::Hide() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

INT_PTR WINAPI MagdaLoginWindow::sDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  MagdaLoginWindow *pThis = (MagdaLoginWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (pThis) {
    return pThis->DialogProc(uMsg, wParam, lParam);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR MagdaLoginWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_LOGIN_BUTTON) {
      OnLogin();
      return 0;
    }
    // Handle Enter key in password field
    if (LOWORD(wParam) == IDC_PASSWORD_INPUT && HIWORD(wParam) == EN_CHANGE) {
      // Check if Enter was pressed (simplified - just check on button click)
      // For now, only login button triggers login
    }
    break;
  case WM_CLOSE:
    Hide();
    return 0;
  }
  return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MagdaLoginWindow::OnLogin() {
  // This is called from the old dialog-based approach
  // For GetUserInputs approach, we call OnLoginWithCredentials directly
  if (!m_hwndEmailInput || !m_hwndPasswordInput) {
    return;
  }

  char email[256] = {0};
  char password[256] = {0};
  GetWindowText(m_hwndEmailInput, email, sizeof(email));
  GetWindowText(m_hwndPasswordInput, password, sizeof(password));

  OnLoginWithCredentials(email, password);
}

void MagdaLoginWindow::OnLoginWithCredentials(const char *email, const char *password) {
  if (!email || !password || strlen(email) == 0 || strlen(password) == 0) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Email and password required\n");
    }
    return;
  }

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Logging in...\n");
  }

  // Use MagdaHTTPClient for login
  static MagdaHTTPClient httpClient;
  WDL_FastString jwt_token, error_msg;

  if (httpClient.SendLoginRequest(email, password, jwt_token, error_msg)) {
    StoreToken(jwt_token.Get());
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Login successful! Token stored.\n");
    }
  } else {
    if (ShowConsoleMsg) {
      char buf[512];
      snprintf(buf, sizeof(buf), "MAGDA: Login failed: %s\n",
               error_msg.GetLength() > 0 ? error_msg.Get() : "Unknown error");
      ShowConsoleMsg(buf);
    }
  }
}

void MagdaLoginWindow::SetStatus(const char *status, bool isError) {
  if (m_hwndStatusLabel && status) {
    SetWindowText(m_hwndStatusLabel, status);
    // Could change text color for errors, but that requires more complex UI
  }
}
