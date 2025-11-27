#pragma once

#ifndef _WIN32
#include <pthread.h>
#endif
#include "../WDL/WDL/jnetlib/httpget.h"
#include "../WDL/WDL/wdlstring.h"
#include "reaper_plugin.h"

// Forward declaration
class JNL_IAsyncDNS;

// HTTP client for MAGDA extension to communicate with backend
// The extension uses this to send questions and receive actions
class MagdaHTTPClient {
public:
  MagdaHTTPClient();
  ~MagdaHTTPClient();

  // Set backend URL (e.g., "http://localhost:8080" or
  // "https://api.musicalaideas.com")
  void SetBackendURL(const char *url);

  // Set JWT token for authentication (optional, for testing without auth)
  void SetJWTToken(const char *token);

  // Send question to backend with current REAPER state
  // Returns true on success, false on error
  // response_json contains the structured JSON response with actions
  // The actions are automatically executed after receiving the response
  bool SendQuestion(const char *question, WDL_FastString &response_json,
                    WDL_FastString &error_msg);

  // Callback type for streaming actions - called for each action as it arrives
  typedef void (*StreamActionCallback)(const char *action_json,
                                       void *user_data);

  // Send question to backend with streaming (SSE)
  // Executes actions one-by-one as they arrive instead of waiting for all
  // callback is called for each action as it's received
  // Returns true on success, false on error
  bool SendQuestionStream(const char *question, StreamActionCallback callback,
                          void *user_data, WDL_FastString &error_msg);

  // Send login request to backend
  // Returns true on success, false on error
  // jwt_token_out contains the JWT token on success
  // error_msg contains error description on failure
  bool SendLoginRequest(const char *email, const char *password,
                        WDL_FastString &jwt_token_out,
                        WDL_FastString &error_msg);

  // Send refresh token request to backend
  // Returns true on success, false on error
  // jwt_token_out contains the new JWT token on success
  // error_msg contains error description on failure
  bool SendRefreshRequest(const char *refresh_token,
                          WDL_FastString &jwt_token_out,
                          WDL_FastString &error_msg);

private:
  WDL_FastString m_backend_url;
  WDL_FastString m_jwt_token;
  JNL_HTTPGet *m_http_get;
  JNL_AsyncDNS *m_dns;

  // Helper to build request JSON with question
  // Note: State is fetched by backend via the extension's HTTP server
  char *BuildRequestJSON(const char *question);

  // Helper to extract actions JSON from response
  // Finds the "actions" field and extracts its value as a JSON string
  static char *ExtractActionsJSON(const char *json_str, int json_len);
};
