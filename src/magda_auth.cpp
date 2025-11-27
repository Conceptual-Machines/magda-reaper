#include "magda_auth.h"
#include "magda_api_client.h"
#include "reaper_plugin.h"
#include <cstring>

#ifndef _WIN32
#include <pthread.h>
#else
#include <windows.h>
#endif

extern reaper_plugin_info_t *g_rec;

// Login thread data structure
struct LoginThreadData {
  char email[256];
  char password[256];
  MagdaAuth::LoginCallback callback;
  bool completed;
  bool success;
  WDL_FastString jwt_token;
  WDL_FastString error_msg;
  HANDLE threadHandle;
};

// Static storage for JWT token
WDL_FastString &MagdaAuth::GetTokenStorage() {
  static WDL_FastString s_jwt_token;
  return s_jwt_token;
}

const char *MagdaAuth::GetStoredToken() {
  // First try to get from persistent storage
  if (g_rec) {
    const char *(*GetExtState)(const char *section, const char *key) =
        (const char *(*)(const char *, const char *))g_rec->GetFunc(
            "GetExtState");
    if (GetExtState) {
      const char *stored = GetExtState("MAGDA", "jwt_token");
      if (stored && strlen(stored) > 0) {
        WDL_FastString &token = GetTokenStorage();
        token.Set(stored);
        return token.Get();
      }
    }
  }

  // Fallback to in-memory storage
  WDL_FastString &token = GetTokenStorage();
  return token.GetLength() > 0 ? token.Get() : nullptr;
}

const char *MagdaAuth::GetStoredRefreshToken() {
  if (g_rec) {
    const char *(*GetExtState)(const char *section, const char *key) =
        (const char *(*)(const char *, const char *))g_rec->GetFunc(
            "GetExtState");
    if (GetExtState) {
      const char *stored = GetExtState("MAGDA", "refresh_token");
      if (stored && strlen(stored) > 0) {
        static WDL_FastString s_refresh_token;
        s_refresh_token.Set(stored);
        return s_refresh_token.Get();
      }
    }
  }
  return nullptr;
}

void MagdaAuth::StoreRefreshToken(const char *token) {
  if (g_rec) {
    void (*SetExtState)(const char *section, const char *key, const char *value,
                        bool persist) =
        (void (*)(const char *, const char *, const char *,
                  bool))g_rec->GetFunc("SetExtState");
    if (SetExtState) {
      SetExtState("MAGDA", "refresh_token", token ? token : "", true);
    }
  }
}

bool MagdaAuth::RefreshToken(WDL_FastString &error_msg) {
  const char *refresh_token = GetStoredRefreshToken();
  if (!refresh_token || !refresh_token[0]) {
    error_msg.Set("No refresh token available");

    // Log for debugging
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: No refresh token found in storage\n");
      }
    }
    return false;
  }

  // Log refresh attempt
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[256];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: Attempting token refresh (refresh token length: %d)\n",
               (int)strlen(refresh_token));
      ShowConsoleMsg(log_msg);
    }
  }

  // Perform refresh request
  static MagdaHTTPClient httpClient;
  WDL_FastString new_token;

  if (!httpClient.SendRefreshRequest(refresh_token, new_token, error_msg)) {
    // Log refresh failure details
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "MAGDA: Token refresh failed: %s\n",
                 error_msg.Get());
        ShowConsoleMsg(log_msg);
      }
    }
    return false;
  }

  // Store new access token
  StoreToken(new_token.Get());

  // Log success
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[256];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: Token refresh successful (new token length: %d)\n",
               (int)new_token.GetLength());
      ShowConsoleMsg(log_msg);
    }
  }

  return true;
}

void MagdaAuth::StoreToken(const char *token) {
  WDL_FastString &storage = GetTokenStorage();
  if (token) {
    storage.Set(token);
  } else {
    storage.Set("");
  }

  // Also persist to Reaper's configuration
  if (g_rec) {
    void (*SetExtState)(const char *section, const char *key, const char *value,
                        bool persist) =
        (void (*)(const char *, const char *, const char *,
                  bool))g_rec->GetFunc("SetExtState");
    if (SetExtState) {
      SetExtState("MAGDA", "jwt_token", token ? token : "", true);
    }
  }
}

// Background thread function for login
#ifdef _WIN32
DWORD WINAPI LoginThreadProc(LPVOID param)
#else
void *LoginThreadProc(void *param)
#endif
{
  LoginThreadData *data = (LoginThreadData *)param;

  // Perform HTTP login request in background thread
  static MagdaHTTPClient httpClient;
  WDL_FastString jwt_token, error_msg;

  data->success = httpClient.SendLoginRequest(data->email, data->password,
                                              jwt_token, error_msg);

  if (data->success) {
    data->jwt_token.Set(jwt_token.Get());
  } else {
    data->error_msg.Set(error_msg.GetLength() > 0 ? error_msg.Get()
                                                  : "Unknown error");
  }

  data->completed = true;

  // Store token if successful (thread-safe for static storage)
  if (data->success) {
    MagdaAuth::StoreToken(data->jwt_token.Get());
    // Note: Refresh token is stored by SendLoginRequest
  }

  // Call callback - the callback should use PostMessage to update UI
  // This ensures UI updates happen on the main thread
  if (data->callback) {
    data->callback(data->success, data->jwt_token.Get(),
                   data->success ? nullptr : data->error_msg.Get());
  }

  // Clean up thread handle
#ifdef _WIN32
  if (data->threadHandle) {
    CloseHandle(data->threadHandle);
  }
#else
  if (data->threadHandle) {
    pthread_t thread = (pthread_t)(INT_PTR)data->threadHandle;
    pthread_detach(thread);
  }
#endif

  // Delete the data structure
  delete data;

#ifdef _WIN32
  return 0;
#else
  return nullptr;
#endif
}

void MagdaAuth::LoginAsync(const char *email, const char *password,
                           LoginCallback callback) {
  if (!email || !password || strlen(email) == 0 || strlen(password) == 0) {
    if (callback) {
      callback(false, nullptr, "Email and password required");
    }
    return;
  }

  // Create thread data on heap
  LoginThreadData *data = new LoginThreadData();
  strncpy(data->email, email, sizeof(data->email) - 1);
  data->email[sizeof(data->email) - 1] = '\0';
  strncpy(data->password, password, sizeof(data->password) - 1);
  data->password[sizeof(data->password) - 1] = '\0';
  data->callback = callback;
  data->completed = false;
  data->success = false;
  data->threadHandle = nullptr;

  // Create background thread
  HANDLE threadHandle = nullptr;
#ifdef _WIN32
  DWORD threadId = 0;
  threadHandle = CreateThread(NULL, 0, LoginThreadProc, data, 0, &threadId);
#else
  pthread_t thread;
  if (pthread_create(&thread, NULL, LoginThreadProc, data) == 0) {
    threadHandle = (HANDLE)(INT_PTR)thread;
  } else {
    if (callback) {
      callback(false, nullptr, "Failed to create login thread");
    }
    delete data;
    return;
  }
#endif

  if (!threadHandle) {
    if (callback) {
      callback(false, nullptr, "Failed to create login thread");
    }
    delete data;
    return;
  }

  // Store thread handle in data so thread can clean itself up
  data->threadHandle = threadHandle;

  // Return immediately - thread will complete in background and call callback
}
