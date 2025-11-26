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
  WDL_FastString &token = GetTokenStorage();
  return token.GetLength() > 0 ? token.Get() : nullptr;
}

void MagdaAuth::StoreToken(const char *token) {
  WDL_FastString &storage = GetTokenStorage();
  if (token) {
    storage.Set(token);
  } else {
    storage.Set("");
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

  data->success = httpClient.SendLoginRequest(data->email, data->password, jwt_token, error_msg);

  if (data->success) {
    data->jwt_token.Set(jwt_token.Get());
  } else {
    data->error_msg.Set(error_msg.GetLength() > 0 ? error_msg.Get() : "Unknown error");
  }

  data->completed = true;

  // Store token if successful (thread-safe for static storage)
  if (data->success) {
    MagdaAuth::StoreToken(data->jwt_token.Get());
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

void MagdaAuth::LoginAsync(const char *email, const char *password, LoginCallback callback) {
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
