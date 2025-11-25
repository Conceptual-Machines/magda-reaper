#include "magda_login_window.h"
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/jnetlib/httpget.h"
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

// Static storage for JWT token
WDL_FastString MagdaLoginWindow::s_jwt_token;

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
  return s_jwt_token.GetLength() > 0 ? s_jwt_token.Get() : nullptr;
}

void MagdaLoginWindow::StoreToken(const char *token) {
  if (token) {
    s_jwt_token.Set(token);
  } else {
    s_jwt_token.Set("");
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
  char retvals[512] = {0};
  bool result = GetUserInputs("MAGDA Login", 2, "Email:,Password:", retvals, sizeof(retvals));

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

  // For now, just show the credentials in console (incremental build - no HTTP call yet)
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg), "MAGDA: Login dialog - Email: %s, Password: [hidden]\n", email);
    ShowConsoleMsg(msg);
    ShowConsoleMsg("MAGDA: HTTP login call will be added next\n");
  }

  // TODO: Add HTTP call in next step
  // OnLoginWithCredentials(email, password);
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
  if (!m_hwndEmailInput || !m_hwndPasswordInput) {
    return;
  }

  char email[256] = {0};
  char password[256] = {0};
  GetWindowText(m_hwndEmailInput, email, sizeof(email));
  GetWindowText(m_hwndPasswordInput, password, sizeof(password));

  if (strlen(email) == 0 || strlen(password) == 0) {
    SetStatus("Please enter email and password", true);
    return;
  }

  SetStatus("Logging in...", false);

  // Build login request JSON
  WDL_FastString json;
  json.Append("{\"email\":\"");
  // Escape email
  for (const char *p = email; *p; p++) {
    if (*p == '"' || *p == '\\') {
      json.Append("\\");
    }
    json.AppendFormatted(1, "%c", *p);
  }
  json.Append("\",\"password\":\"");
  // Escape password
  for (const char *p = password; *p; p++) {
    if (*p == '"' || *p == '\\') {
      json.Append("\\");
    }
    json.AppendFormatted(1, "%c", *p);
  }
  json.Append("\"}");

  // Make HTTP request to login endpoint
  static JNL_AsyncDNS *dns = new JNL_AsyncDNS;
  JNL_HTTPGet *http_get = new JNL_HTTPGet(dns);

  // Get backend URL (same as chat client)
  static MagdaHTTPClient tempClient;
  const char *backend_url = "http://localhost:8080"; // TODO: Get from config

  WDL_FastString url;
  url.Set(backend_url);
  url.Append("/api/auth/login");

  char content_length[128];
  snprintf(content_length, sizeof(content_length), "Content-Length: %d\r\n", json.GetLength());
  http_get->addheader("Content-Type: application/json");
  http_get->addheader(content_length);

  http_get->connect(url.Get(), 1, "POST");

  JNL_IConnection *con = http_get->get_con();
  if (!con) {
    SetStatus("Failed to connect", true);
    delete http_get;
    return;
  }

  // Wait for connection
  while (con->get_state() == JNL_Connection::STATE_CONNECTING ||
         con->get_state() == JNL_Connection::STATE_RESOLVING) {
    con->run();
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000); // 10ms
#endif
  }

  if (con->get_state() != JNL_Connection::STATE_CONNECTED) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Connection failed\n");
    }
    delete http_get;
    return;
  }

  // Send POST body
  int sent = 0;
  int json_len = json.GetLength();
  while (sent < json_len) {
    con->run();
    int available = con->send_bytes_available();
    if (available > 0) {
      int to_send = json_len - sent;
      if (to_send > available)
        to_send = available;
      int result = con->send(json.Get() + sent, to_send);
      if (result < 0) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Failed to send login request\n");
        }
        delete http_get;
        return;
      }
      sent += result;
    } else {
#ifdef _WIN32
      Sleep(10);
#else
      usleep(10000);
#endif
    }
  }

  // Receive response
  while (http_get->get_status() < 2) {
    int result = http_get->run();
    if (result < 0) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Login request failed\n");
      }
      delete http_get;
      return;
    }
    if (result == 1) {
      break;
    }
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
  }

  int reply_code = http_get->getreplycode();
  if (reply_code != 200) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char buf[256];
      snprintf(buf, sizeof(buf), "MAGDA: Login failed (HTTP %d)\n", reply_code);
      ShowConsoleMsg(buf);
    }
    delete http_get;
    return;
  }

  // Read response
  WDL_FastString response;
  char buffer[4096];
  while (http_get->get_status() == 2) {
    int available = http_get->bytes_available();
    if (available > 0) {
      int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
      int read = http_get->get_bytes(buffer, to_read);
      if (read > 0) {
        buffer[read] = '\0';
        response.Append(buffer, read);
      }
    } else {
      int result = http_get->run();
      if (result < 0) {
        break;
      }
#ifdef _WIN32
      Sleep(10);
#else
      usleep(10000);
#endif
    }
  }

  // Parse response to extract access_token
  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(response.Get(), (int)response.GetLength());
  if (parser.m_err || !root) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Failed to parse login response\n");
    }
    delete http_get;
    return;
  }

  wdl_json_element *token_elem = root->get_item_by_name("access_token");
  if (!token_elem || !token_elem->m_value_string) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Login failed: Invalid response\n");
    }
    delete http_get;
    return;
  }

  const char *token = token_elem->m_value;
  if (token) {
    StoreToken(token);
    SetStatus("Login successful!", false);

    // Clear password field
    if (m_hwndPasswordInput) {
      SetWindowText(m_hwndPasswordInput, "");
    }

    // Hide window after successful login
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000); // 1 second
#endif
    Hide();
  } else {
    SetStatus("Login failed: No token received", true);
  }

  delete http_get;
}

void MagdaLoginWindow::SetStatus(const char *status, bool isError) {
  if (m_hwndStatusLabel && status) {
    SetWindowText(m_hwndStatusLabel, status);
    // Could change text color for errors, but that requires more complex UI
  }
}
