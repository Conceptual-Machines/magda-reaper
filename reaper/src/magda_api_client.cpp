#include "magda_api_client.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "../WDL/WDL/jnetlib/asyncdns.h"
#include "../WDL/WDL/jsonparse.h"
#include "../WDL/WDL/timing.h"
#include "magda_actions.h"
#include "magda_state.h"
#include <cstring>

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
  m_backend_url.Set("https://api.musicalaideas.com");
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
    free(state_json);
  } else {
    json.Append("{}");
  }

  json.Append("}");

  // Allocate and return
  int len = json.GetLength();
  char *result = (char *)malloc(len + 1);
  if (result) {
    memcpy(result, json.Get(), len);
    result[len] = '\0';
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
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
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
            }
          }

          if (depth == 0 && p > value_start) {
            // Extract the actions JSON
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

bool MagdaHTTPClient::SendQuestion(const char *question, WDL_FastString &response_json,
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
  url.Append("/api/v1/magda/chat"); // Backend endpoint

  // Create HTTP client
  if (m_http_get) {
    delete m_http_get;
  }
  m_http_get = new JNL_HTTPGet((JNL_IAsyncDNS *)m_dns);

  // Set headers
  char content_length_header[128];
  snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %d\r\n",
           request_json_len);
  m_http_get->addheader("Content-Type: application/json");
  m_http_get->addheader(content_length_header);

  // Add JWT token if set
  if (m_jwt_token.GetLength() > 0) {
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_jwt_token.Get());
    m_http_get->addheader(auth_header);
  }

  // Connect with POST method
  m_http_get->connect(url.Get(), 1, "POST");

  // Get connection and send POST body
  JNL_IConnection *con = m_http_get->get_con();
  if (!con) {
    free(request_json);
    error_msg.Set("Failed to get connection");
    return false;
  }

  // Run connection until connected
  while (con->get_state() == JNL_Connection::STATE_CONNECTING ||
         con->get_state() == JNL_Connection::STATE_RESOLVING) {
    con->run();
    SLEEP_MS(10); // Small delay
  }

  if (con->get_state() != JNL_Connection::STATE_CONNECTED) {
    free(request_json);
    error_msg.Set("Failed to connect");
    return false;
  }

  // Wait for HTTP headers to be sent, then send POST body
  int header_wait_ms = 0;
  while (header_wait_ms < 1000) {
    con->run();
    SLEEP_MS(10);
    header_wait_ms += 10;
    if (con->send_bytes_available() > 0) {
      break;
    }
  }

  // Send POST body using send_string (works better than send())
  con->send_string(request_json);

  free(request_json);

  // Run HTTP client to receive response
  while (m_http_get->get_status() < 2) {
    int result = m_http_get->run();
    if (result < 0) {
      error_msg.Set(m_http_get->geterrorstr() ? m_http_get->geterrorstr() : "HTTP request failed");
      return false;
    }
    if (result == 1) {
      // Connection closed
      break;
    }
    SLEEP_MS(10);
  }

  // Check response code
  int reply_code = m_http_get->getreplycode();
  if (reply_code != 200) {
    char buf[256];
    snprintf(buf, sizeof(buf), "HTTP error %d", reply_code);
    error_msg.Set(buf);
    return false;
  }

  // Read response body
  char buffer[4096];
  int total_read = 0;
  while (m_http_get->get_status() == 2) {
    int available = m_http_get->bytes_available();
    if (available > 0) {
      int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
      int read = m_http_get->get_bytes(buffer, to_read);
      if (read > 0) {
        buffer[read] = '\0';
        response_json.Append(buffer, read);
        total_read += read;
      }
    } else {
      int result = m_http_get->run();
      if (result < 0) {
        break;
      }
      SLEEP_MS(10);
    }
  }

  // Read any remaining data
  while (m_http_get->bytes_available() > 0) {
    int available = m_http_get->bytes_available();
    int to_read = available > (int)sizeof(buffer) - 1 ? (int)sizeof(buffer) - 1 : available;
    int read = m_http_get->get_bytes(buffer, to_read);
    if (read > 0) {
      buffer[read] = '\0';
      response_json.Append(buffer, read);
    } else {
      break;
    }
  }

  // Execute actions from the response
  // The backend returns JSON with an "actions" field containing an array of actions
  if (response_json.GetLength() > 0) {
    WDL_FastString execution_result, execution_error;

    // Try to extract actions JSON from response
    char *actions_json = ExtractActionsJSON(response_json.Get(), (int)response_json.GetLength());

    if (actions_json) {
      // Execute the extracted actions
      if (!MagdaActions::ExecuteActions(actions_json, execution_result, execution_error)) {
        // Log error but don't fail the request
        // Could append execution_error to error_msg if needed
      }
      free(actions_json);
    } else {
      // No "actions" field found - maybe the response IS the actions array/object
      // Try executing the whole response
      wdl_json_parser parser;
      wdl_json_element *root = parser.parse(response_json.Get(), (int)response_json.GetLength());
      if (!parser.m_err && root && (root->is_array() || root->is_object())) {
        if (!MagdaActions::ExecuteActions(response_json.Get(), execution_result, execution_error)) {
          // Log error but don't fail the request
        }
      }
    }
  }

  return true;
}

// Platform-specific HTTPS POST request helpers
#ifdef _WIN32
// Windows: WinHTTP implementation
static bool SendHTTPSRequest_WinHTTP(const char *url, const char *post_data, int post_data_len,
                                     WDL_FastString &response, WDL_FastString &error_msg) {
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
  hSession = WinHttpOpen(L"MAGDA Reaper Extension/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    error_msg.Set("Failed to initialize WinHTTP");
    return false;
  }

  // Connect to server
  int port = urlComp.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
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

  hRequest = WinHttpOpenRequest(hConnect, L"POST", path_w, nullptr, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  delete[] path_w;

  if (!hRequest) {
    error_msg.Set("Failed to create request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Set headers
  wchar_t headers[] = L"Content-Type: application/json\r\n";
  WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

  // Send request
  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)post_data,
                          post_data_len, post_data_len, 0)) {
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
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                      WINHTTP_NO_HEADER_INDEX);

  if (statusCode != 200) {
    char error_buf[256];
    snprintf(error_buf, sizeof(error_buf), "HTTP error %lu", statusCode);
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

  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
    buffer = (char *)realloc(buffer, bytesRead + bytesAvailable + 1);
    if (!buffer) {
      error_msg.Set("Failed to allocate memory");
      break;
    }
    if (!WinHttpReadData(hRequest, buffer + bytesRead, bytesAvailable, &bytesRead)) {
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

static size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  CurlWriteData *data = (CurlWriteData *)userp;
  if (data->response) {
    data->response->Append((const char *)contents, (int)realsize);
  }
  return realsize;
}

static bool SendHTTPSRequest_Curl(const char *url, const char *post_data, int post_data_len,
                                  WDL_FastString &response, WDL_FastString &error_msg) {
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
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  long response_code = 0;
  if (res == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
      char error_buf[256];
      snprintf(error_buf, sizeof(error_buf), "HTTP error %ld", response_code);
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

bool MagdaHTTPClient::SendLoginRequest(const char *email, const char *password,
                                       WDL_FastString &jwt_token_out, WDL_FastString &error_msg) {
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
  if (!SendHTTPSRequest_WinHTTP(url.Get(), request_json.Get(), request_json_len, response,
                                error_msg)) {
    return false;
  }
#else
  if (!SendHTTPSRequest_Curl(url.Get(), request_json.Get(), request_json_len, response,
                             error_msg)) {
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
  // Response format: {"token": "..."} or {"access_token": "..."}
  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(response_str, (int)strlen(response_str));
  if (parser.m_err || !root) {
    error_msg.Set("Failed to parse response JSON");
    return false;
  }

  // Try "token" first, then "access_token"
  wdl_json_element *token_elem = root->get_item_by_name("token");
  if (!token_elem || !token_elem->m_value_string) {
    token_elem = root->get_item_by_name("access_token");
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
  return true;
}
