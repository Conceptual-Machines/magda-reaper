#include "magda_api_client.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/timing.h"
#include "magda_actions.h"
#include "magda_auth.h"
#include "magda_env.h"
#include "magda_imgui_login.h"
#include "magda_state.h"
#include "reaper_plugin.h"
#include <cstring>

extern reaper_plugin_info_t *g_rec;

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <curl/curl.h>
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

MagdaHTTPClient::MagdaHTTPClient() : m_http_get(nullptr), m_dns(nullptr) {
  m_dns = new JNL_AsyncDNS;

  // Check for backend URL from settings first, then environment variable
  const char *backend_url = MagdaImGuiLogin::GetBackendURL();
  if (backend_url && strlen(backend_url) > 0) {
    m_backend_url.Set(backend_url);
  } else {
    m_backend_url.Set("https://api.musicalaideas.com");
  }

#ifndef _WIN32
  // Initialize libcurl on first use (thread-safe)
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}

MagdaHTTPClient::~MagdaHTTPClient() {
  if (m_http_get) {
    delete m_http_get;
  }
  if (m_dns) {
    delete m_dns;
  }
}

void MagdaHTTPClient::SetBackendURL(const char *url) {
  if (url) {
    m_backend_url.Set(url);
  }
}

void MagdaHTTPClient::SetJWTToken(const char *token) {
  if (token) {
    m_jwt_token.Set(token);
  } else {
    m_jwt_token.Set("");
  }
}

char *MagdaHTTPClient::BuildRequestJSON(const char *question) {
  WDL_FastString json;
  json.Append("{\"question\":");

  // Escape question string
  json.Append("\"");
  const char *p = question;
  while (p && *p) {
    switch (*p) {
    case '"':
      json.Append("\\\"");
      break;
    case '\\':
      json.Append("\\\\");
      break;
    case '\n':
      json.Append("\\n");
      break;
    case '\r':
      json.Append("\\r");
      break;
    case '\t':
      json.Append("\\t");
      break;
    default:
      json.Append(p, 1);
      break;
    }
    p++;
  }
  json.Append("\"");

  // Add REAPER state snapshot
  json.Append(",\"state\":");
  char *state_json = MagdaState::GetStateSnapshot();
  if (state_json) {
    json.Append(state_json);

    // Log state JSON for debugging (truncate if too long)
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        int state_len = (int)strlen(state_json);
        int preview_len = state_len > 500 ? 500 : state_len;
        char log_msg[1024];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: State JSON (%d bytes): %.*s%s\n", state_len,
                 preview_len, state_json, state_len > 500 ? "..." : "");
        ShowConsoleMsg(log_msg);
      }
    }

    free(state_json);
  } else {
    json.Append("{}");
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Warning - GetStateSnapshot returned null\n");
      }
    }
  }

  json.Append("}");

  // Allocate and return
  int len = json.GetLength();
  char *result = (char *)malloc(len + 1);
  if (result) {
    memcpy(result, json.Get(), len);
    result[len] = '\0';

    // Log full request JSON for debugging (truncate if too long)
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        int preview_len = len > 1000 ? 1000 : len;
        char log_msg[2048];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: Request JSON (%d bytes): %.*s%s\n", len, preview_len,
                 result, len > 1000 ? "..." : "");
        ShowConsoleMsg(log_msg);
      }
    }
  }
  return result;
}

char *MagdaHTTPClient::ExtractActionsJSON(const char *json_str, int json_len) {
  if (!json_str || json_len <= 0) {
    return nullptr;
  }

  // Find "actions" key in JSON
  const char *actions_key = "\"actions\"";
  int actions_key_len = (int)strlen(actions_key);
  const char *p = json_str;
  const char *end = json_str + json_len;

  // Search for "actions" key
  while (p < end - actions_key_len) {
    if (strncmp(p, actions_key, actions_key_len) == 0) {
      p += actions_key_len;
      // Skip whitespace
      while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
      }
      // Find colon
      if (p < end && *p == ':') {
        p++;
        // Skip whitespace after colon
        while (p < end &&
               (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
          p++;
        }
        // Now p points to the start of the actions value
        const char *value_start = p;

        // Find the end of the JSON value (array or object)
        // We need to match brackets/braces to find the end
        if (p < end && (*p == '[' || *p == '{')) {
          char open_char = *p;
          char close_char = (open_char == '[') ? ']' : '}';
          int depth = 1;
          p++;

          while (p < end && depth > 0) {
            if (*p == '\\' && p + 1 < end) {
              // Skip escaped character
              p += 2;
              continue;
            }
            if (*p == '"') {
              // Skip string
              p++;
              while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) {
                  p += 2;
                } else {
                  p++;
                }
              }
              if (p < end)
                p++;
              continue;
            }
            if (*p == open_char) {
              depth++;
            } else if (*p == close_char) {
              depth--;
            }
            if (depth > 0) {
              p++;
            } else if (depth == 0) {
              // Include the closing bracket/brace
              p++;
              break;
            }
          }

          if (depth == 0 && p > value_start) {
            // Extract the actions JSON (p now points after the closing bracket)
            int actions_len = (int)(p - value_start);
            char *result = (char *)malloc(actions_len + 1);
            if (result) {
              memcpy(result, value_start, actions_len);
              result[actions_len] = '\0';
              return result;
            }
          }
        }
      }
      break;
    }
    p++;
  }

  return nullptr;
}

// Platform-specific HTTPS POST request helpers
// These must be defined before SendQuestion which uses them
#ifdef _WIN32
// Windows: WinHTTP implementation
static bool SendHTTPSRequest_WinHTTP(const char *url, const char *post_data,
                                     int post_data_len,
                                     WDL_FastString &response,
                                     WDL_FastString &error_msg,
                                     const char *auth_token = nullptr) {
  HINTERNET hSession = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;
  bool success = false;

  // Parse URL
  URL_COMPONENTSA urlComp;
  ZeroMemory(&urlComp, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwSchemeLength = (DWORD)-1;
  urlComp.dwHostNameLength = (DWORD)-1;
  urlComp.dwUrlPathLength = (DWORD)-1;

  char scheme[16] = {0};
  char hostname[256] = {0};
  char path[512] = {0};
  urlComp.lpszScheme = scheme;
  urlComp.lpszHostName = hostname;
  urlComp.lpszUrlPath = path;

  if (!InternetCrackUrlA(url, (DWORD)strlen(url), 0, &urlComp)) {
    error_msg.Set("Failed to parse URL");
    return false;
  }

  // Initialize WinHTTP
  hSession = WinHttpOpen(L"MAGDA Reaper Extension/1.0",
                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    error_msg.Set("Failed to initialize WinHTTP");
    return false;
  }

  // Set timeout (in milliseconds)
  DWORD timeout_ms = (DWORD)(timeout_seconds * 1000);
  WinHttpSetTimeouts(hSession, 30000, 30000, timeout_ms, timeout_ms);

  // Connect to server
  int port = urlComp.nScheme == INTERNET_SCHEME_HTTPS
                 ? INTERNET_DEFAULT_HTTPS_PORT
                 : INTERNET_DEFAULT_HTTP_PORT;
  if (urlComp.nPort != 0) {
    port = urlComp.nPort;
  }

  // Convert hostname to wide string
  int hostname_len = MultiByteToWideChar(CP_UTF8, 0, hostname, -1, nullptr, 0);
  wchar_t *hostname_w = new wchar_t[hostname_len];
  MultiByteToWideChar(CP_UTF8, 0, hostname, -1, hostname_w, hostname_len);

  hConnect = WinHttpConnect(hSession, hostname_w, port, 0);
  delete[] hostname_w;

  if (!hConnect) {
    error_msg.Set("Failed to connect to server");
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Create request
  int path_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  wchar_t *path_w = new wchar_t[path_len];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, path_len);

  hRequest = WinHttpOpenRequest(
      hConnect, L"POST", path_w, nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  delete[] path_w;

  if (!hRequest) {
    error_msg.Set("Failed to create request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Set timeout (in milliseconds)
  DWORD timeout_ms = (DWORD)(timeout_seconds * 1000);
  WinHttpSetTimeouts(hSession, 30000, 30000, timeout_ms, timeout_ms);

  // Set headers
  wchar_t headers[1024];
  if (auth_token && strlen(auth_token) > 0) {
    // Convert auth token to wide string
    int auth_len = MultiByteToWideChar(CP_UTF8, 0, auth_token, -1, nullptr, 0);
    wchar_t *auth_w = new wchar_t[auth_len + 100];
    swprintf(auth_w, auth_len + 100,
             L"Content-Type: application/json\r\nAuthorization: Bearer %S\r\n",
             auth_token);
    WinHttpAddRequestHeaders(hRequest, auth_w, (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
    delete[] auth_w;
  } else {
    wcscpy(headers, L"Content-Type: application/json\r\n");
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  // Send request
  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          (LPVOID)post_data, post_data_len, post_data_len, 0)) {
    error_msg.Set("Failed to send request");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Receive response
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    error_msg.Set("Failed to receive response");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Read response data
  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

  if (statusCode != 200) {
    // Read response body for error details
    DWORD bytesAvailable = 0;
    char *error_buffer = nullptr;
    DWORD bytesRead = 0;

    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
           bytesAvailable > 0) {
      error_buffer =
          (char *)realloc(error_buffer, bytesRead + bytesAvailable + 1);
      if (!error_buffer)
        break;
      if (!WinHttpReadData(hRequest, error_buffer + bytesRead, bytesAvailable,
                           &bytesRead)) {
        break;
      }
      bytesRead += bytesAvailable;
    }

    char error_buf[512];
    if (error_buffer && bytesRead > 0) {
      error_buffer[bytesRead] = '\0';
      int len = bytesRead > 200 ? 200 : bytesRead;
      snprintf(error_buf, sizeof(error_buf), "HTTP error %lu: %.200s",
               statusCode, error_buffer);
      free(error_buffer);
    } else {
      snprintf(error_buf, sizeof(error_buf), "HTTP error %lu", statusCode);
    }
    error_msg.Set(error_buf);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Read response body
  DWORD bytesAvailable = 0;
  char *buffer = nullptr;
  DWORD bytesRead = 0;

  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
         bytesAvailable > 0) {
    buffer = (char *)realloc(buffer, bytesRead + bytesAvailable + 1);
    if (!buffer) {
      error_msg.Set("Failed to allocate memory");
      break;
    }
    if (!WinHttpReadData(hRequest, buffer + bytesRead, bytesAvailable,
                         &bytesRead)) {
      error_msg.Set("Failed to read response data");
      break;
    }
    bytesRead += bytesAvailable;
  }

  if (buffer) {
    buffer[bytesRead] = '\0';
    response.Set(buffer);
    free(buffer);
    success = true;
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return success;
}
#else
// macOS/Linux: libcurl implementation
struct CurlWriteData {
  WDL_FastString *response;
};

static size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb,
                                void *userp) {
  size_t realsize = size * nmemb;
  CurlWriteData *data = (CurlWriteData *)userp;
  if (data->response) {
    data->response->Append((const char *)contents, (int)realsize);
  }
  return realsize;
}

static bool SendHTTPSRequest_Curl(const char *url, const char *post_data,
                                  int post_data_len, WDL_FastString &response,
                                  WDL_FastString &error_msg,
                                  const char *auth_token = nullptr,
                                  int timeout_seconds = 30) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    return false;
  }

  CurlWriteData writeData;
  writeData.response = &response;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data_len);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (auth_token && strlen(auth_token) > 0) {
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             auth_token);
    headers = curl_slist_append(headers, auth_header);

    // Log the Authorization header (first 50 chars of token for debugging)
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[512];
        int token_preview_len =
            strlen(auth_token) > 50 ? 50 : (int)strlen(auth_token);
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: Authorization header: Bearer %.50s...\n", auth_token);
        ShowConsoleMsg(log_msg);
      }
    }
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Log request details before sending
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: Sending POST request to %s (timeout: %d seconds, body "
               "size: %d bytes)\n",
               url, timeout_seconds, post_data_len);
      ShowConsoleMsg(log_msg);
    }
  }

  CURLcode res = curl_easy_perform(curl);

  // Log curl result for debugging
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[512];
      if (res == CURLE_OK) {
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: curl_easy_perform succeeded\n");
      } else {
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: curl_easy_perform failed: %s\n",
                 curl_easy_strerror(res));
      }
      ShowConsoleMsg(log_msg);
    }
  }

  long response_code = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Log response code and size
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: HTTP response code: %ld, body size: %d bytes\n",
                 response_code, (int)response.GetLength());
        ShowConsoleMsg(log_msg);
      }
    }

    if (response_code != 200) {
      // Response body is already in 'response' from the write callback
      char error_buf[512];
      if (response.GetLength() > 0) {
        int body_len =
            response.GetLength() > 200 ? 200 : (int)response.GetLength();
        snprintf(error_buf, sizeof(error_buf), "HTTP error %ld: %.200s",
                 response_code, response.Get());
      } else {
        snprintf(error_buf, sizeof(error_buf), "HTTP error %ld", response_code);
      }
      error_msg.Set(error_buf);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return false;
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return false;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return true;
}
#endif

bool MagdaHTTPClient::SendPOSTRequest(const char *endpoint,
                                      const char *json_data,
                                      WDL_FastString &response,
                                      WDL_FastString &error_msg,
                                      int timeout_seconds) {
  if (!endpoint || !json_data) {
    error_msg.Set("Invalid parameters");
    return false;
  }

  // Build full URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append(endpoint);

  // Get auth token (try to get from storage if not set)
  const char *auth_token =
      m_jwt_token.GetLength() > 0 ? m_jwt_token.Get() : nullptr;
  if (!auth_token) {
    auth_token = MagdaAuth::GetStoredToken();
    if (auth_token && strlen(auth_token) > 0) {
      m_jwt_token.Set(auth_token);
    }
  }

  // Use default timeout if not specified
  if (timeout_seconds == 0) {
    timeout_seconds = 30;
  }

  // Use platform-specific HTTPS implementation
  bool success;
#ifdef _WIN32
  success = SendHTTPSRequest_WinHTTP(url.Get(), json_data,
                                     (int)strlen(json_data), response,
                                     error_msg, auth_token, timeout_seconds);
#else
  success =
      SendHTTPSRequest_Curl(url.Get(), json_data, (int)strlen(json_data),
                            response, error_msg, auth_token, timeout_seconds);
#endif

  // If we got 401, try refreshing token and retry once
  if (!success && error_msg.GetLength() > 0 && strstr(error_msg.Get(), "401")) {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Token expired, attempting refresh...\n");
      }
    }

    WDL_FastString refresh_error;
    if (MagdaAuth::RefreshToken(refresh_error)) {
      // Update token and retry
      const char *new_token = MagdaAuth::GetStoredToken();
      if (new_token && strlen(new_token) > 0) {
        m_jwt_token.Set(new_token);
        response.Set("");
        error_msg.Set("");

        // Retry the request
#ifdef _WIN32
        success = SendHTTPSRequest_WinHTTP(
            url.Get(), json_data, (int)strlen(json_data), response, error_msg,
            new_token, timeout_seconds);
#else
        success = SendHTTPSRequest_Curl(url.Get(), json_data,
                                        (int)strlen(json_data), response,
                                        error_msg, new_token, timeout_seconds);
#endif
        if (success && g_rec) {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA: Token refreshed, request succeeded\n");
          }
        }
      }
    } else {
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(msg, sizeof(msg), "MAGDA: Token refresh failed: %s\n",
                   refresh_error.Get());
          ShowConsoleMsg(msg);
        }
      }
    }
  }

  return success;
}

bool MagdaHTTPClient::SendQuestion(const char *question,
                                   WDL_FastString &response_json,
                                   WDL_FastString &error_msg) {
  if (!question || !question[0]) {
    error_msg.Set("Empty question");
    return false;
  }

  // Build request JSON
  char *request_json = BuildRequestJSON(question);
  if (!request_json) {
    error_msg.Set("Failed to build request JSON");
    return false;
  }

  int request_json_len = (int)strlen(request_json);

  // Build URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/api/v1/chat"); // Backend endpoint

  // Use platform-specific HTTPS implementation
  WDL_FastString response;
  // Get auth token (try to get from storage if not set)
  const char *auth_token =
      m_jwt_token.GetLength() > 0 ? m_jwt_token.Get() : nullptr;
  if (!auth_token) {
    auth_token = MagdaAuth::GetStoredToken();
    if (auth_token && strlen(auth_token) > 0) {
      m_jwt_token.Set(auth_token);
    }
  }

  // Log the request for debugging
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char log_msg[1024];
      snprintf(log_msg, sizeof(log_msg), "MAGDA: Sending request to %s\n",
               url.Get());
      ShowConsoleMsg(log_msg);
      if (auth_token) {
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: Using JWT token (length: %d)\n",
                 (int)m_jwt_token.GetLength());
        ShowConsoleMsg(log_msg);
      } else {
        ShowConsoleMsg("MAGDA: No JWT token set\n");
      }
      // Log request body (truncate if too long)
      int body_len = request_json_len;
      if (body_len > 500) {
        body_len = 500;
      }
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: Request body (%d bytes): %.500s%s\n", request_json_len,
               request_json, body_len < request_json_len ? "..." : "");
      ShowConsoleMsg(log_msg);
    }
  }

  bool success;
#ifdef _WIN32
  success = SendHTTPSRequest_WinHTTP(url.Get(), request_json, request_json_len,
                                     response, error_msg, auth_token);
#else
  success = SendHTTPSRequest_Curl(url.Get(), request_json, request_json_len,
                                  response, error_msg, auth_token);
#endif

  // If we got 401, try refreshing token and retry once
  if (!success && error_msg.GetLength() > 0 && strstr(error_msg.Get(), "401")) {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Token expired, attempting refresh...\n");
      }
    }

    WDL_FastString refresh_error;
    if (MagdaAuth::RefreshToken(refresh_error)) {
      // Update token and retry
      const char *new_token = MagdaAuth::GetStoredToken();
      if (new_token && strlen(new_token) > 0) {
        m_jwt_token.Set(new_token);
        response.Set("");
        error_msg.Set("");

        // Retry the request
#ifdef _WIN32
        success =
            SendHTTPSRequest_WinHTTP(url.Get(), request_json, request_json_len,
                                     response, error_msg, new_token);
#else
        success =
            SendHTTPSRequest_Curl(url.Get(), request_json, request_json_len,
                                  response, error_msg, new_token);
#endif
        if (success && g_rec) {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA: Token refreshed, request succeeded\n");
          }
        }
      }
    } else {
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(msg, sizeof(msg), "MAGDA: Token refresh failed: %s\n",
                   refresh_error.Get());
          ShowConsoleMsg(msg);
        }
      }
    }
  }

  free(request_json);

  if (!success) {
    return false;
  }

  // Log response for debugging
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      if (error_msg.GetLength() > 0) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "MAGDA: Request failed: %s\n",
                 error_msg.Get());
        ShowConsoleMsg(log_msg);
      } else if (response.GetLength() > 0) {
        char log_msg[512];
        int len = response.GetLength();
        if (len > 200)
          len = 200;
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: Response received (%d bytes): %.200s...\n",
                 (int)response.GetLength(), response.Get());
        ShowConsoleMsg(log_msg);
      }
    }
  }

  // Parse response
  if (response.GetLength() > 0) {
    response_json.Set(response.Get());
  } else {
    error_msg.Set("Empty response from server");
    return false;
  }

  // Execute actions from the response
  // The backend returns JSON with an "actions" field containing an array of
  // actions
  if (response_json.GetLength() > 0) {
    WDL_FastString execution_result, execution_error;

    // Try to extract actions JSON from response
    char *actions_json =
        ExtractActionsJSON(response_json.Get(), (int)response_json.GetLength());

    if (actions_json) {
      // Log extracted actions for debugging
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char log_msg[512];
          snprintf(log_msg, sizeof(log_msg),
                   "MAGDA: Extracted actions JSON: %s\n", actions_json);
          ShowConsoleMsg(log_msg);
        }
      }

      // Execute the extracted actions
      if (!MagdaActions::ExecuteActions(actions_json, execution_result,
                                        execution_error)) {
        // Log error
        if (g_rec) {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                     "MAGDA: Action execution failed: %s\n",
                     execution_error.Get());
            ShowConsoleMsg(log_msg);
          }
        }
      } else {
        // Log success
        if (g_rec) {
          void (*ShowConsoleMsg)(const char *msg) =
              (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
          if (ShowConsoleMsg) {
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                     "MAGDA: Actions executed successfully: %s\n",
                     execution_result.Get());
            ShowConsoleMsg(log_msg);
          }
        }
      }
      free(actions_json);
    } else {
      // No "actions" field found - maybe the response IS the actions
      // array/object Try executing the whole response
      wdl_json_parser parser;
      wdl_json_element *root =
          parser.parse(response_json.Get(), (int)response_json.GetLength());
      if (!parser.m_err && root && (root->is_array() || root->is_object())) {
        if (!MagdaActions::ExecuteActions(response_json.Get(), execution_result,
                                          execution_error)) {
          // Log error
          if (g_rec) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              char log_msg[512];
              snprintf(log_msg, sizeof(log_msg),
                       "MAGDA: Action execution failed (fallback): %s\n",
                       execution_error.Get());
              ShowConsoleMsg(log_msg);
            }
          }
        } else {
          // Log success
          if (g_rec) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              char log_msg[512];
              snprintf(log_msg, sizeof(log_msg),
                       "MAGDA: Actions executed successfully (fallback): %s\n",
                       execution_result.Get());
              ShowConsoleMsg(log_msg);
            }
          }
        }
      }
    }
  }

  return true;
}

bool MagdaHTTPClient::SendLoginRequest(const char *email, const char *password,
                                       WDL_FastString &jwt_token_out,
                                       WDL_FastString &error_msg) {
  if (!email || !email[0] || !password || !password[0]) {
    error_msg.Set("Email and password are required.");
    return false;
  }

  // Build login request JSON
  WDL_FastString request_json;
  request_json.Append("{\"email\":\"");
  // Escape email
  for (const char *p = email; *p; p++) {
    if (*p == '"' || *p == '\\') {
      request_json.Append("\\");
    }
    request_json.AppendFormatted(1, "%c", *p);
  }
  request_json.Append("\",\"password\":\"");
  // Escape password
  for (const char *p = password; *p; p++) {
    if (*p == '"' || *p == '\\') {
      request_json.Append("\\");
    }
    request_json.AppendFormatted(1, "%c", *p);
  }
  request_json.Append("\"}");

  int request_json_len = (int)request_json.GetLength();

  // Build URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/api/auth/login");

  // Use platform-specific HTTPS implementation
  WDL_FastString response;
#ifdef _WIN32
  if (!SendHTTPSRequest_WinHTTP(url.Get(), request_json.Get(), request_json_len,
                                response, error_msg, nullptr)) {
    return false;
  }
#else
  if (!SendHTTPSRequest_Curl(url.Get(), request_json.Get(), request_json_len,
                             response, error_msg, nullptr)) {
    return false;
  }
#endif

  // Parse response JSON
  const char *response_str = response.Get();
  if (!response_str || !response_str[0]) {
    error_msg.Set("Empty response from server");
    return false;
  }

  // Extract JWT token from response
  // Response format: {"access_token": "...", "refresh_token": "..."}
  wdl_json_parser parser;
  wdl_json_element *root =
      parser.parse(response_str, (int)strlen(response_str));
  if (parser.m_err || !root) {
    error_msg.Set("Failed to parse response JSON");
    return false;
  }

  // Try "access_token" first, then "token" for backwards compatibility
  wdl_json_element *token_elem = root->get_item_by_name("access_token");
  if (!token_elem || !token_elem->m_value_string) {
    token_elem = root->get_item_by_name("token");
  }

  if (!token_elem || !token_elem->m_value_string) {
    error_msg.Set("No token found in response");
    return false;
  }

  const char *token = token_elem->m_value;
  if (!token || !token[0]) {
    error_msg.Set("Token is empty");
    return false;
  }

  jwt_token_out.Set(token);

  // Also extract and store refresh_token if present
  wdl_json_element *refresh_elem = root->get_item_by_name("refresh_token");
  if (refresh_elem && refresh_elem->m_value_string &&
      refresh_elem->m_value[0]) {
    // Store refresh token via MagdaAuth
    MagdaAuth::StoreRefreshToken(refresh_elem->m_value);
  }

  return true;
}

bool MagdaHTTPClient::SendRefreshRequest(const char *refresh_token,
                                         WDL_FastString &jwt_token_out,
                                         WDL_FastString &error_msg) {
  if (!refresh_token || !refresh_token[0]) {
    error_msg.Set("Refresh token is required");
    return false;
  }

  // Build refresh request JSON
  WDL_FastString request_json;
  request_json.Append("{\"refresh_token\":\"");
  // Escape refresh token
  for (const char *p = refresh_token; *p; p++) {
    if (*p == '"' || *p == '\\') {
      request_json.Append("\\");
    }
    request_json.AppendFormatted(1, "%c", *p);
  }
  request_json.Append("\"}");

  int request_json_len = (int)request_json.GetLength();

  // Build URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/api/auth/refresh");

  // Use platform-specific HTTPS implementation
  WDL_FastString response;
#ifdef _WIN32
  if (!SendHTTPSRequest_WinHTTP(url.Get(), request_json.Get(), request_json_len,
                                response, error_msg, nullptr)) {
    return false;
  }
#else
  if (!SendHTTPSRequest_Curl(url.Get(), request_json.Get(), request_json_len,
                             response, error_msg, nullptr)) {
    return false;
  }
#endif

  // Parse response JSON
  const char *response_str = response.Get();
  if (!response_str || !response_str[0]) {
    error_msg.Set("Empty response from server");

    // Log for debugging
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Refresh request returned empty response\n");
      }
    }
    return false;
  }

  // Log response for debugging
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      int preview_len =
          strlen(response_str) > 200 ? 200 : (int)strlen(response_str);
      char log_msg[512];
      snprintf(log_msg, sizeof(log_msg),
               "MAGDA: Refresh response (%d bytes): %.*s%s\n",
               (int)strlen(response_str), preview_len, response_str,
               strlen(response_str) > 200 ? "..." : "");
      ShowConsoleMsg(log_msg);
    }
  }

  // Extract new tokens from response
  // Response format: {"access_token": "...", "refresh_token": "..."}
  wdl_json_parser parser;
  wdl_json_element *root =
      parser.parse(response_str, (int)strlen(response_str));
  if (parser.m_err || !root) {
    char parse_error[256];
    snprintf(parse_error, sizeof(parse_error),
             "Failed to parse response JSON: %s",
             parser.m_err ? parser.m_err : "unknown error");
    error_msg.Set(parse_error);

    // Log parse error
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "MAGDA: JSON parse error: %s\n",
                 parse_error);
        ShowConsoleMsg(log_msg);
      }
    }
    return false;
  }

  // Extract access_token
  wdl_json_element *token_elem = root->get_item_by_name("access_token");
  if (!token_elem || !token_elem->m_value_string) {
    error_msg.Set("No access_token found in response");
    return false;
  }

  const char *token = token_elem->m_value;
  if (!token || !token[0]) {
    error_msg.Set("Access token is empty");
    return false;
  }

  jwt_token_out.Set(token);

  // Also extract and store new refresh_token if present
  wdl_json_element *refresh_elem = root->get_item_by_name("refresh_token");
  if (refresh_elem && refresh_elem->m_value_string &&
      refresh_elem->m_value[0]) {
    MagdaAuth::StoreRefreshToken(refresh_elem->m_value);
  }

  return true;
}

#ifndef _WIN32
// Static write callback function for curl (required for C function pointer)
// This processes SSE events line by line
struct CurlStreamData {
  MagdaHTTPClient::StreamActionCallback callback;
  void *user_data;
  WDL_FastString *error_msg;
  bool success;
  char line_buffer[8192];
  int line_pos;
  WDL_FastString accumulated_data; // For storing response body on errors
};

static size_t curl_stream_write_callback(char *ptr, size_t size, size_t nmemb,
                                         void *userdata) {
  if (!userdata || !ptr) {
    return 0; // Safety check
  }
  CurlStreamData *data = (CurlStreamData *)userdata;
  size_t total_size = size * nmemb;

  // Accumulate all data for error handling
  data->accumulated_data.Append(ptr, (int)total_size);

  for (size_t i = 0; i < total_size; i++) {
    if (ptr[i] == '\n' ||
        data->line_pos >= (int)sizeof(data->line_buffer) - 1) {
      data->line_buffer[data->line_pos] = '\0';
      if (data->line_pos > 0) {
        // Process SSE line
        if (strncmp(data->line_buffer, "data: ", 6) == 0) {
          const char *json_data = data->line_buffer + 6;

          // Log received data for debugging
          if (g_rec) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              int preview_len =
                  strlen(json_data) > 150 ? 150 : (int)strlen(json_data);
              char log_msg[256];
              snprintf(log_msg, sizeof(log_msg),
                       "MAGDA: SSE data (%d bytes): %.*s%s\n",
                       (int)strlen(json_data), preview_len, json_data,
                       strlen(json_data) > 150 ? "..." : "");
              ShowConsoleMsg(log_msg);
            }
          }

          // API sends actions wrapped in an object with "type": "action" and
          // "action": {...} Check if it's a control event (done/error) or an
          // action
          wdl_json_parser parser;
          wdl_json_element *root =
              parser.parse(json_data, (int)strlen(json_data));
          if (!parser.m_err && root) {
            wdl_json_element *type_elem = root->get_item_by_name("type");
            if (type_elem && type_elem->m_value_string) {
              const char *event_type = type_elem->m_value;

              if (strcmp(event_type, "action") == 0) {
                // This is an action - extract the "action" field
                wdl_json_element *action_elem =
                    root->get_item_by_name("action");
                if (action_elem) {
                  // Serialize the action object back to JSON string
                  // We need to get the raw JSON for the action object
                  // Find where the "action" field starts in the original JSON
                  const char *action_start = strstr(json_data, "\"action\"");
                  if (action_start) {
                    // Find the opening brace of the action object
                    const char *brace = strchr(action_start, '{');
                    if (brace) {
                      // Find the matching closing brace
                      int brace_count = 0;
                      const char *end = brace;
                      for (; *end; end++) {
                        if (*end == '{')
                          brace_count++;
                        else if (*end == '}') {
                          brace_count--;
                          if (brace_count == 0) {
                            // Found the end of the action object
                            int action_len = (int)(end - brace + 1);
                            // Allocate buffer for action JSON
                            char *action_json = (char *)malloc(action_len + 1);
                            if (action_json) {
                              memcpy(action_json, brace, action_len);
                              action_json[action_len] = '\0';

                              if (g_rec) {
                                void (*ShowConsoleMsg)(const char *msg) =
                                    (void (*)(const char *))g_rec->GetFunc(
                                        "ShowConsoleMsg");
                                if (ShowConsoleMsg) {
                                  ShowConsoleMsg("MAGDA: Extracted action "
                                                 "JSON, calling callback\n");
                                }
                              }

                              if (data->callback) {
                                data->callback(action_json, data->user_data);
                              }

                              free(action_json);
                            }
                            break;
                          }
                        }
                      }
                    }
                  }
                }
              } else if (strcmp(event_type, "done") == 0) {
                // Control event: done
                if (g_rec) {
                  void (*ShowConsoleMsg)(const char *msg) =
                      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
                  if (ShowConsoleMsg) {
                    ShowConsoleMsg("MAGDA: Control event: done\n");
                  }
                }
                data->success = true;
              } else if (strcmp(event_type, "error") == 0) {
                // Control event: error
                if (g_rec) {
                  void (*ShowConsoleMsg)(const char *msg) =
                      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
                  if (ShowConsoleMsg) {
                    ShowConsoleMsg("MAGDA: Control event: error\n");
                  }
                }
                wdl_json_element *msg_elem = root->get_item_by_name("message");
                if (msg_elem && msg_elem->m_value_string) {
                  data->error_msg->Set(msg_elem->m_value);
                }
              }
            } else {
              // No "type" field - this might be raw action JSON, use it
              // directly
              if (g_rec) {
                void (*ShowConsoleMsg)(const char *msg) =
                    (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
                if (ShowConsoleMsg) {
                  ShowConsoleMsg(
                      "MAGDA: No type field, treating as raw action JSON\n");
                }
              }
              if (data->callback) {
                data->callback(json_data, data->user_data);
              }
            }
          } else {
            // Parse failed - might still be valid action JSON, try passing it
            // anyway (API should send valid JSON, but be defensive)
            if (g_rec) {
              void (*ShowConsoleMsg)(const char *msg) =
                  (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
              if (ShowConsoleMsg) {
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                         "MAGDA: JSON parse failed: %s, trying as raw action\n",
                         parser.m_err ? parser.m_err : "unknown error");
                ShowConsoleMsg(log_msg);
              }
            }
            if (json_data[0] == '{' && data->callback) {
              data->callback(json_data, data->user_data);
            }
          }
        } else {
          // Log non-data lines for debugging
          if (g_rec && data->line_pos > 0 && strlen(data->line_buffer) > 0) {
            void (*ShowConsoleMsg)(const char *msg) =
                (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
            if (ShowConsoleMsg) {
              int preview_len = strlen(data->line_buffer) > 50
                                    ? 50
                                    : (int)strlen(data->line_buffer);
              char log_msg[256];
              snprintf(log_msg, sizeof(log_msg),
                       "MAGDA: SSE line (not data:): %.*s%s\n", preview_len,
                       data->line_buffer,
                       strlen(data->line_buffer) > 50 ? "..." : "");
              ShowConsoleMsg(log_msg);
            }
          }
        }
      }
      data->line_pos = 0;
    } else if (ptr[i] != '\r') {
      data->line_buffer[data->line_pos++] = ptr[i];
    }
  }

  return total_size;
}
#endif

bool MagdaHTTPClient::SendQuestionStream(const char *question,
                                         StreamActionCallback callback,
                                         void *user_data,
                                         WDL_FastString &error_msg) {
  if (!question || !question[0]) {
    error_msg.Set("Empty question");
    return false;
  }

  if (!callback) {
    error_msg.Set("Callback required for streaming");
    return false;
  }

  // Build request JSON
  char *request_json = BuildRequestJSON(question);
  if (!request_json) {
    error_msg.Set("Failed to build request JSON");
    return false;
  }

  int request_json_len = (int)strlen(request_json);

  // Build URL - use non-streaming endpoint (has CFG support for DSL generation)
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/api/v1/chat"); // Use non-streaming endpoint with CFG

  // Use platform-specific HTTPS implementation for streaming
  const char *auth_token =
      m_jwt_token.GetLength() > 0 ? m_jwt_token.Get() : nullptr;

#ifdef _WIN32
  // Windows: WinHTTP streaming implementation
  HINTERNET hSession = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;
  bool success = false;

  // Parse URL
  URL_COMPONENTSA urlComp;
  ZeroMemory(&urlComp, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwSchemeLength = (DWORD)-1;
  urlComp.dwHostNameLength = (DWORD)-1;
  urlComp.dwUrlPathLength = (DWORD)-1;

  char scheme[16] = {0};
  char hostname[256] = {0};
  char path[512] = {0};
  urlComp.lpszScheme = scheme;
  urlComp.lpszHostName = hostname;
  urlComp.lpszUrlPath = path;

  if (!InternetCrackUrlA(url.Get(), (DWORD)url.GetLength(), 0, &urlComp)) {
    error_msg.Set("Failed to parse URL");
    free(request_json);
    return false;
  }

  // Initialize WinHTTP
  hSession = WinHttpOpen(L"MAGDA Reaper Extension/1.0",
                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    error_msg.Set("Failed to initialize WinHTTP");
    free(request_json);
    return false;
  }

  // Connect to server
  int port = urlComp.nScheme == INTERNET_SCHEME_HTTPS
                 ? INTERNET_DEFAULT_HTTPS_PORT
                 : INTERNET_DEFAULT_HTTP_PORT;
  if (urlComp.nPort != 0) {
    port = urlComp.nPort;
  }

  int hostname_len = MultiByteToWideChar(CP_UTF8, 0, hostname, -1, nullptr, 0);
  wchar_t *hostname_w = new wchar_t[hostname_len];
  MultiByteToWideChar(CP_UTF8, 0, hostname, -1, hostname_w, hostname_len);

  hConnect = WinHttpConnect(hSession, hostname_w, port, 0);
  delete[] hostname_w;

  if (!hConnect) {
    error_msg.Set("Failed to connect to server");
    WinHttpCloseHandle(hSession);
    free(request_json);
    return false;
  }

  // Create request
  int path_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  wchar_t *path_w = new wchar_t[path_len];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, path_len);

  hRequest = WinHttpOpenRequest(
      hConnect, L"POST", path_w, nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  delete[] path_w;

  if (!hRequest) {
    error_msg.Set("Failed to create request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    free(request_json);
    return false;
  }

  // Set headers
  if (auth_token && strlen(auth_token) > 0) {
    int auth_len = MultiByteToWideChar(CP_UTF8, 0, auth_token, -1, nullptr, 0);
    wchar_t *auth_w = new wchar_t[auth_len + 100];
    swprintf(auth_w, auth_len + 100,
             L"Content-Type: application/json\r\nAuthorization: Bearer "
             L"%S\r\nAccept: "
             L"text/event-stream\r\n",
             auth_token);
    WinHttpAddRequestHeaders(hRequest, auth_w, (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
    delete[] auth_w;
  } else {
    wchar_t headers[] =
        L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  // Send request
  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          (LPVOID)request_json, request_json_len,
                          request_json_len, 0)) {
    error_msg.Set("Failed to send request");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    free(request_json);
    return false;
  }

  // Receive response
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    error_msg.Set("Failed to receive response");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    free(request_json);
    return false;
  }

  // Check status code
  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

  if (statusCode != 200) {
    // Read response body for error details
    DWORD bytesAvailable = 0;
    WDL_FastString error_response_body;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
           bytesAvailable > 0) {
      char *buffer = (char *)malloc(bytesAvailable + 1);
      if (!buffer)
        break;
      DWORD bytesRead = 0;
      if (WinHttpReadData(hRequest, buffer, bytesAvailable, &bytesRead)) {
        buffer[bytesRead] = '\0';
        error_response_body.Append(buffer, bytesRead);
      }
      free(buffer);
    }

    char error_buf[1024];
    if (error_response_body.GetLength() > 0) {
      snprintf(error_buf, sizeof(error_buf), "HTTP error %lu: %.500s",
               statusCode, error_response_body.Get());
    } else {
      snprintf(error_buf, sizeof(error_buf), "HTTP error %lu", statusCode);
    }
    error_msg.Set(error_buf);

    // Log error details
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg(error_buf);
        ShowConsoleMsg("\n");
      }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    free(request_json);
    return false;
  }

  // Read stream line by line (SSE format)
  char line_buffer[8192];
  int line_pos = 0;
  DWORD bytesAvailable = 0;
  char read_buffer[4096];
  DWORD bytesRead = 0;

  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
         bytesAvailable > 0) {
    if (bytesAvailable > sizeof(read_buffer)) {
      bytesAvailable = sizeof(read_buffer);
    }

    if (!WinHttpReadData(hRequest, read_buffer, bytesAvailable, &bytesRead)) {
      break;
    }

    // Process received data line by line
    for (DWORD i = 0; i < bytesRead; i++) {
      if (read_buffer[i] == '\n' || line_pos >= (int)sizeof(line_buffer) - 1) {
        line_buffer[line_pos] = '\0';
        if (line_pos > 0) {
          // Process SSE line
          if (strncmp(line_buffer, "data: ", 6) == 0) {
            const char *json_data = line_buffer + 6;

            // API now sends action JSON directly (no wrapper) to minimize
            // parsing work Check if it's a control event (has "type" field) or
            // an action (raw action JSON)
            wdl_json_parser parser;
            wdl_json_element *root =
                parser.parse(json_data, (int)strlen(json_data));
            if (!parser.m_err && root) {
              wdl_json_element *type_elem = root->get_item_by_name("type");
              if (type_elem && type_elem->m_value_string) {
                // Control event (done/error)
                const char *event_type = type_elem->m_value;
                if (strcmp(event_type, "done") == 0) {
                  success = true;
                  break;
                } else if (strcmp(event_type, "error") == 0) {
                  wdl_json_element *msg_elem =
                      root->get_item_by_name("message");
                  if (msg_elem && msg_elem->m_value_string) {
                    error_msg.Set(msg_elem->m_value);
                  }
                  break;
                }
              } else {
                // No "type" field - this is raw action JSON, use it directly
                // API sends action JSON directly, so we can pass it to callback
                // immediately
                callback(json_data, user_data);
              }
            } else {
              // Parse failed - might still be valid action JSON, try passing it
              // anyway (API should send valid JSON, but be defensive)
              if (json_data[0] == '{') {
                callback(json_data, user_data);
              }
            }
          }
        }
        line_pos = 0;
      } else if (read_buffer[i] != '\r') {
        line_buffer[line_pos++] = read_buffer[i];
      }
    }
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  free(request_json);
  return success;

#else
  // macOS/Linux: libcurl streaming implementation
  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    free(request_json);
    return false;
  }

  CurlStreamData stream_data;
  stream_data.callback = callback;
  stream_data.user_data = user_data;
  stream_data.error_msg = &error_msg;
  stream_data.success = false;
  stream_data.line_pos = 0;
  stream_data.line_buffer[0] = '\0';

  curl_easy_setopt(curl, CURLOPT_URL, url.Get());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_json_len);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_stream_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_data);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: text/event-stream");
  if (auth_token && strlen(auth_token) > 0) {
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             auth_token);
    headers = curl_slist_append(headers, auth_header);

    // Log token usage for debugging
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "MAGDA: Using JWT token (length: %d) for streaming request\n",
                 (int)strlen(auth_token));
        ShowConsoleMsg(log_msg);
      }
    }
  } else {
    // Log missing token
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg(
            "MAGDA: WARNING - No JWT token set for streaming request\n");
      }
    }
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  long response_code = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
      // For non-200 responses, use the accumulated data from the write callback
      const char *error_body = stream_data.accumulated_data.Get();
      int error_body_len = stream_data.accumulated_data.GetLength();

      // Try to parse error response as JSON to extract error message
      if (error_body && error_body_len > 0) {
        wdl_json_parser parser;
        wdl_json_element *root = parser.parse(error_body, error_body_len);
        if (!parser.m_err && root) {
          wdl_json_element *error_elem = root->get_item_by_name("error");
          if (error_elem && error_elem->m_value_string) {
            char error_buf[1024];
            snprintf(error_buf, sizeof(error_buf), "HTTP error %ld: %s",
                     response_code, error_elem->m_value);
            error_msg.Set(error_buf);
          } else {
            // No "error" field, use full response body (truncated)
            int preview_len = error_body_len > 500 ? 500 : error_body_len;
            char error_buf[1024];
            snprintf(error_buf, sizeof(error_buf), "HTTP error %ld: %.*s%s",
                     response_code, preview_len, error_body,
                     error_body_len > 500 ? "..." : "");
            error_msg.Set(error_buf);
          }
        } else {
          // Not JSON or parse failed, use raw response (truncated)
          int preview_len = error_body_len > 500 ? 500 : error_body_len;
          char error_buf[1024];
          snprintf(error_buf, sizeof(error_buf), "HTTP error %ld: %.*s%s",
                   response_code, preview_len, error_body,
                   error_body_len > 500 ? "..." : "");
          error_msg.Set(error_buf);
        }
      } else {
        char error_buf[512];
        snprintf(error_buf, sizeof(error_buf), "HTTP error %ld", response_code);
        error_msg.Set(error_buf);
      }

      // Log error details
      if (g_rec) {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
          char log_msg[2048];
          snprintf(log_msg, sizeof(log_msg),
                   "MAGDA: Request failed with HTTP %ld\n", response_code);
          ShowConsoleMsg(log_msg);
          if (error_body && error_body_len > 0) {
            int preview_len = error_body_len > 1000 ? 1000 : error_body_len;
            snprintf(log_msg, sizeof(log_msg),
                     "MAGDA: Error response (%d bytes): %.*s%s\n",
                     error_body_len, preview_len, error_body,
                     error_body_len > 1000 ? "..." : "");
            ShowConsoleMsg(log_msg);
          }
        }
      }

      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      free(request_json);
      return false;
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(request_json);
    return false;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(request_json);
  return stream_data.success;
#endif
}

bool MagdaHTTPClient::CheckHealth(WDL_FastString &error_msg,
                                  int timeout_seconds) {
#ifndef _WIN32
  // Use curl for macOS/Linux
  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    return false;
  }

  // Build health check URL
  WDL_FastString url;
  url.Set(m_backend_url.Get());
  url.Append("/health");

  curl_easy_setopt(curl, CURLOPT_URL, url.Get());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                   timeout_seconds > 0 ? timeout_seconds : 5);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  // We don't care about the response body, just if we can connect
  curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
  curl_easy_setopt(
      curl, CURLOPT_WRITEFUNCTION,
      +[](char *, size_t size, size_t nmemb, void *) -> size_t {
        return size * nmemb; // Discard body
      });

  CURLcode res = curl_easy_perform(curl);

  if (res == CURLE_OK) {
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (response_code >= 200 && response_code < 300) {
      return true;
    } else {
      char err[64];
      snprintf(err, sizeof(err), "HTTP %ld", response_code);
      error_msg.Set(err);
      return false;
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return false;
  }
#else
  // Windows implementation placeholder
  error_msg.Set("Health check not implemented on Windows");
  return false;
#endif
}
