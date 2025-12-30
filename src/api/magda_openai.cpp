#include "magda_openai.h"
#include "../WDL/WDL/jsonparse.h"
#include "../dsl/magda_dsl_grammar.h"
#include "../dsl/magda_jsfx_grammar.h"
#include "reaper_plugin.h"
#include <cstdlib>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

// ============================================================================
// Global instance
// ============================================================================

static MagdaOpenAI *s_openai_instance = nullptr;

MagdaOpenAI *GetMagdaOpenAI() {
  if (!s_openai_instance) {
    s_openai_instance = new MagdaOpenAI();
  }
  return s_openai_instance;
}

// ============================================================================
// MagdaOpenAI Implementation
// ============================================================================

MagdaOpenAI::MagdaOpenAI() : m_timeout_seconds(60) {
  m_model.Set("gpt-5.1"); // GPT-5.1 for DSL generation (CFG)
}

MagdaOpenAI::~MagdaOpenAI() {}

void MagdaOpenAI::SetAPIKey(const char *api_key) {
  if (api_key) {
    m_api_key.Set(api_key);
  }
}

void MagdaOpenAI::SetModel(const char *model) {
  if (model) {
    m_model.Set(model);
  }
}

// Helper to escape JSON string
static void EscapeJSONString(const char *input, WDL_FastString &output) {
  output.Set("");
  while (input && *input) {
    switch (*input) {
    case '"':
      output.Append("\\\"");
      break;
    case '\\':
      output.Append("\\\\");
      break;
    case '\n':
      output.Append("\\n");
      break;
    case '\r':
      output.Append("\\r");
      break;
    case '\t':
      output.Append("\\t");
      break;
    default:
      output.Append(input, 1);
      break;
    }
    input++;
  }
}

char *MagdaOpenAI::BuildRequestJSON(const char *question, const char *system_prompt,
                                    const char *state_json) {
  WDL_FastString json;
  WDL_FastString escaped;

  json.Set("{");

  // Model
  json.Append("\"model\":\"");
  json.Append(m_model.Get());
  json.Append("\",");

  // Input messages
  json.Append("\"input\":[");

  // User message with question
  json.Append("{\"role\":\"user\",\"content\":\"");
  EscapeJSONString(question, escaped);
  json.Append(escaped.Get());
  json.Append("\"}");

  // Add state if provided
  if (state_json && *state_json) {
    json.Append(",{\"role\":\"user\",\"content\":\"Current REAPER state: ");
    EscapeJSONString(state_json, escaped);
    json.Append(escaped.Get());
    json.Append("\"}");
  }

  json.Append("],");

  // System prompt (instructions)
  json.Append("\"instructions\":\"");
  EscapeJSONString(system_prompt ? system_prompt : "", escaped);
  json.Append(escaped.Get());
  json.Append("\",");

  // Text format - plain text for CFG output
  json.Append("\"text\":{\"format\":{\"type\":\"text\"}},");

  // Tools array with CFG grammar tool (custom type with grammar format)
  json.Append("\"tools\":[{");
  json.Append("\"type\":\"custom\",");
  json.Append("\"name\":\"magda_dsl\",");
  json.Append("\"description\":\"");
  EscapeJSONString(MAGDA_DSL_TOOL_DESCRIPTION, escaped);
  json.Append(escaped.Get());
  json.Append("\",");
  json.Append("\"format\":{");
  json.Append("\"type\":\"grammar\",");
  json.Append("\"syntax\":\"lark\",");
  json.Append("\"definition\":\"");
  EscapeJSONString(MAGDA_DSL_GRAMMAR, escaped);
  json.Append(escaped.Get());
  json.Append("\"}}],");

  // Disable parallel tool calls (CFG tools don't support it)
  json.Append("\"parallel_tool_calls\":false");

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

bool MagdaOpenAI::ExtractDSLFromResponse(const char *response_json, int response_len,
                                         WDL_FastString &out_dsl, WDL_FastString &error_msg) {
  if (!response_json || response_len <= 0) {
    error_msg.Set("Empty response from API");
    return false;
  }

  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(response_json, response_len);

  if (parser.m_err || !root) {
    error_msg.SetFormatted(256, "Failed to parse API response: %s",
                           parser.m_err ? parser.m_err : "unknown error");
    return false;
  }

  // Check for API error (only if error field is an object with a message, not
  // null) When error is null, error_elem exists but has no children
  // (get_item_by_name returns nullptr)
  wdl_json_element *error_elem = root->get_item_by_name("error");
  if (error_elem) {
    wdl_json_element *msg_elem = error_elem->get_item_by_name("message");
    if (msg_elem && msg_elem->m_value_string && msg_elem->m_value) {
      error_msg.SetFormatted(512, "OpenAI API error: %s", msg_elem->m_value);
      return false;
    }
    // If error exists but has no message, check if it's an actual error object
    // (has children)
    if (error_elem->m_array && error_elem->m_array->GetSize() > 0) {
      error_msg.Set("OpenAI API returned an error");
      return false;
    }
    // Otherwise it's "error": null - not an error, continue
  }

  // Extract token usage from response
  m_lastTokenUsage = TokenUsage{}; // Reset
  wdl_json_element *usage_elem = root->get_item_by_name("usage");
  if (usage_elem) {
    wdl_json_element *input_tokens = usage_elem->get_item_by_name("input_tokens");
    wdl_json_element *output_tokens = usage_elem->get_item_by_name("output_tokens");
    wdl_json_element *total_tokens = usage_elem->get_item_by_name("total_tokens");
    // WDL JSON parser stores numbers as raw string values
    if (input_tokens && input_tokens->m_value && !input_tokens->m_value_string)
      m_lastTokenUsage.input_tokens = atoi(input_tokens->m_value);
    if (output_tokens && output_tokens->m_value && !output_tokens->m_value_string)
      m_lastTokenUsage.output_tokens = atoi(output_tokens->m_value);
    if (total_tokens && total_tokens->m_value && !total_tokens->m_value_string)
      m_lastTokenUsage.total_tokens = atoi(total_tokens->m_value);
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char buf[256];
        snprintf(buf, sizeof(buf), "MAGDA: Token usage - input=%d, output=%d, total=%d\n",
                 m_lastTokenUsage.input_tokens, m_lastTokenUsage.output_tokens,
                 m_lastTokenUsage.total_tokens);
        ShowConsoleMsg(buf);
      }
    }
  } else {
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: No 'usage' field found in API response\n");
      }
    }
  }

  // Navigate to output array
  wdl_json_element *output = root->get_item_by_name("output");
  if (!output || !output->is_array()) {
    error_msg.Set("Response missing 'output' array");
    return false;
  }

  // Search for custom_tool_call in output items (CFG grammar response)
  int output_count = output->m_array ? output->m_array->GetSize() : 0;
  for (int i = 0; i < output_count; i++) {
    wdl_json_element *item = output->enum_item(i);
    if (!item)
      continue;

    wdl_json_element *type_elem = item->get_item_by_name("type");
    if (!type_elem || !type_elem->m_value_string)
      continue;

    // CFG tool calls have type "custom_tool_call"
    if (strcmp(type_elem->m_value, "custom_tool_call") == 0) {
      // Check if this is our magda_dsl tool
      wdl_json_element *name_elem = item->get_item_by_name("name");
      if (name_elem && name_elem->m_value_string && strcmp(name_elem->m_value, "magda_dsl") == 0) {
        // DSL code is directly in the "input" field (raw text, not JSON)
        wdl_json_element *input_elem = item->get_item_by_name("input");
        if (input_elem && input_elem->m_value_string && input_elem->m_value[0]) {
          out_dsl.Set(input_elem->m_value);
          return true;
        }
      }
    }
  }

  // Fallback: check for text output (in case CFG wasn't used)
  for (int i = 0; i < output_count; i++) {
    wdl_json_element *item = output->enum_item(i);
    if (!item)
      continue;

    wdl_json_element *type_elem = item->get_item_by_name("type");
    if (!type_elem || !type_elem->m_value_string)
      continue;

    if (strcmp(type_elem->m_value, "message") == 0) {
      wdl_json_element *content = item->get_item_by_name("content");
      if (content && content->is_array()) {
        int content_count = content->m_array ? content->m_array->GetSize() : 0;
        for (int j = 0; j < content_count; j++) {
          wdl_json_element *content_item = content->enum_item(j);
          if (!content_item)
            continue;

          wdl_json_element *text_elem = content_item->get_item_by_name("text");
          if (text_elem && text_elem->m_value_string) {
            // Check if this looks like DSL
            const char *text = text_elem->m_value;
            if (strstr(text, "track(") || strstr(text, "filter(")) {
              out_dsl.Set(text);
              return true;
            }
          }
        }
      }
    }
  }

  error_msg.Set("No DSL output found in API response");
  return false;
}

// ============================================================================
// HTTP Implementation
// ============================================================================

#ifdef _WIN32
// Windows WinHTTP implementation
bool MagdaOpenAI::SendHTTPSRequest(const char *url, const char *post_data, int post_data_len,
                                   WDL_FastString &response, WDL_FastString &error_msg) {
  HINTERNET hSession = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;
  bool success = false;

  // Parse URL
  URL_COMPONENTSA urlComp;
  ZeroMemory(&urlComp, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);

  char hostname[256] = {0};
  char path[512] = {0};
  urlComp.lpszHostName = hostname;
  urlComp.dwHostNameLength = sizeof(hostname);
  urlComp.lpszUrlPath = path;
  urlComp.dwUrlPathLength = sizeof(path);

  if (!InternetCrackUrlA(url, (DWORD)strlen(url), 0, &urlComp)) {
    error_msg.Set("Failed to parse URL");
    return false;
  }

  hSession = WinHttpOpen(L"MAGDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                         WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    error_msg.Set("Failed to initialize WinHTTP");
    return false;
  }

  // Set timeout
  DWORD timeout_ms = m_timeout_seconds * 1000;
  WinHttpSetTimeouts(hSession, 30000, 30000, timeout_ms, timeout_ms);

  // Convert hostname to wide string
  int hostname_len = MultiByteToWideChar(CP_UTF8, 0, hostname, -1, nullptr, 0);
  wchar_t *hostname_w = new wchar_t[hostname_len];
  MultiByteToWideChar(CP_UTF8, 0, hostname, -1, hostname_w, hostname_len);

  hConnect = WinHttpConnect(hSession, hostname_w, INTERNET_DEFAULT_HTTPS_PORT, 0);
  delete[] hostname_w;

  if (!hConnect) {
    error_msg.Set("Failed to connect");
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Convert path to wide string
  int path_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  wchar_t *path_w = new wchar_t[path_len];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, path_len);

  hRequest = WinHttpOpenRequest(hConnect, L"POST", path_w, nullptr, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  delete[] path_w;

  if (!hRequest) {
    error_msg.Set("Failed to create request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Set headers
  wchar_t headers[1024];
  swprintf(headers, 1024, L"Content-Type: application/json\r\nAuthorization: Bearer %S\r\n",
           m_api_key.Get());
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

  // Check status code
  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                      WINHTTP_NO_HEADER_INDEX);

  // Read response body
  DWORD bytesAvailable = 0;
  while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
    char *buffer = (char *)malloc(bytesAvailable + 1);
    if (!buffer)
      break;

    DWORD bytesRead = 0;
    if (WinHttpReadData(hRequest, buffer, bytesAvailable, &bytesRead)) {
      buffer[bytesRead] = '\0';
      response.Append(buffer, bytesRead);
    }
    free(buffer);
  }

  if (statusCode != 200) {
    error_msg.SetFormatted(512, "HTTP error %lu: %.200s", statusCode, response.Get());
  } else {
    success = true;
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return success;
}

#else
// macOS/Linux curl implementation
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

// Streaming callback data structure for SSE processing
struct CurlStreamWriteData {
  MagdaOpenAI::StreamCallback callback;
  char line_buffer[16384];
  int line_pos;
  bool success;
  bool received_content; // Track if we received any text content
  WDL_FastString error_msg;
};

// SSE streaming write callback - processes OpenAI streaming events
static size_t CurlStreamWriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  CurlStreamWriteData *data = (CurlStreamWriteData *)userp;
  char *ptr = (char *)contents;

  for (size_t i = 0; i < total_size; i++) {
    if (ptr[i] == '\n' || data->line_pos >= (int)sizeof(data->line_buffer) - 1) {
      data->line_buffer[data->line_pos] = '\0';

      if (data->line_pos > 0) {
        // Process SSE line
        if (strncmp(data->line_buffer, "data: ", 6) == 0) {
          const char *json_data = data->line_buffer + 6;

          // Skip [DONE] marker
          if (strcmp(json_data, "[DONE]") == 0) {
            data->success = true;
            // Call callback with is_done = true
            if (data->callback) {
              data->callback("", true);
            }
          } else {
            // Parse JSON to extract text delta
            wdl_json_parser parser;
            wdl_json_element *root = parser.parse(json_data, (int)strlen(json_data));

            if (!parser.m_err && root) {
              wdl_json_element *type_elem = root->get_item_by_name("type");
              if (type_elem && type_elem->m_value_string) {
                const char *event_type = type_elem->m_value;

                // Handle text delta events (for plain text output)
                if (strcmp(event_type, "response.output_text.delta") == 0) {
                  wdl_json_element *delta_elem = root->get_item_by_name("delta");
                  if (delta_elem && delta_elem->m_value_string && delta_elem->m_value) {
                    // Call callback with text chunk
                    if (data->callback) {
                      data->callback(delta_elem->m_value, false);
                    }
                    data->received_content = true;
                  }
                } else if (strcmp(event_type, "response.function_call_arguments.delta") == 0) {
                  // CFG grammar tool call streaming (for JSFX/DSL output)
                  wdl_json_element *delta_elem = root->get_item_by_name("delta");
                  if (delta_elem && delta_elem->m_value_string && delta_elem->m_value) {
                    if (data->callback) {
                      data->callback(delta_elem->m_value, false);
                    }
                    data->received_content = true;
                  }
                } else if (strcmp(event_type, "response.output_text.done") == 0 ||
                           strcmp(event_type, "response.function_call_arguments.done") == 0) {
                  // Text/arguments output complete - mark content received
                  data->received_content = true;
                } else if (strcmp(event_type, "response.done") == 0 ||
                           strcmp(event_type, "response.completed") == 0) {
                  // Stream complete
                  data->success = true;
                  if (data->callback) {
                    data->callback("", true);
                  }
                } else if (strcmp(event_type, "response.failed") == 0) {
                  // Response failed - extract error from response.error.message
                  wdl_json_element *response_elem = root->get_item_by_name("response");
                  if (response_elem) {
                    wdl_json_element *error_elem = response_elem->get_item_by_name("error");
                    if (error_elem) {
                      wdl_json_element *msg_elem = error_elem->get_item_by_name("message");
                      if (msg_elem && msg_elem->m_value_string && msg_elem->m_value) {
                        data->error_msg.Set(msg_elem->m_value);
                      }
                    }
                  }
                } else if (strcmp(event_type, "error") == 0) {
                  // Error event
                  wdl_json_element *msg_elem = root->get_item_by_name("message");
                  if (msg_elem && msg_elem->m_value_string && msg_elem->m_value) {
                    data->error_msg.Set(msg_elem->m_value);
                  }
                }
              }
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

bool MagdaOpenAI::SendHTTPSRequest(const char *url, const char *post_data, int post_data_len,
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
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)m_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  // Set headers
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_api_key.Get());
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  bool success = false;
  if (res == CURLE_OK) {
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Log response for debugging
    if (g_rec) {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char log_msg[2048];
        snprintf(log_msg, sizeof(log_msg), "MAGDA OpenAI: HTTP %ld, Response: %.1500s\n",
                 response_code, response.Get());
        ShowConsoleMsg(log_msg);
      }
    }

    if (response_code == 200) {
      success = true;
    } else {
      error_msg.SetFormatted(512, "HTTP error %ld: %.200s", response_code, response.Get());
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return success;
}
#endif

// ============================================================================
// Public API Methods
// ============================================================================

bool MagdaOpenAI::GenerateDSL(const char *question, const char *system_prompt,
                              WDL_FastString &out_dsl, WDL_FastString &error_msg) {
  return GenerateDSLWithState(question, system_prompt, nullptr, out_dsl, error_msg);
}

bool MagdaOpenAI::GenerateDSLWithState(const char *question, const char *system_prompt,
                                       const char *state_json, WDL_FastString &out_dsl,
                                       WDL_FastString &error_msg) {
  if (!HasAPIKey()) {
    error_msg.Set("OpenAI API key not configured");
    return false;
  }

  if (!question || !*question) {
    error_msg.Set("Empty question");
    return false;
  }

  // Log request
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA OpenAI: Generating DSL for: %.100s%s\n", question,
               strlen(question) > 100 ? "..." : "");
      ShowConsoleMsg(msg);
    }
  }

  // Build request JSON
  char *request_json = BuildRequestJSON(question, system_prompt, state_json);
  if (!request_json) {
    error_msg.Set("Failed to build request JSON");
    return false;
  }

  // Log request for debugging (truncated)
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[2048];
      snprintf(msg, sizeof(msg), "MAGDA OpenAI: Request JSON (first 1000 chars): %.1000s%s\n",
               request_json, strlen(request_json) > 1000 ? "..." : "");
      ShowConsoleMsg(msg);
    }
  }

  // Make API request
  WDL_FastString response;
  bool success = SendHTTPSRequest("https://api.openai.com/v1/responses", request_json,
                                  (int)strlen(request_json), response, error_msg);
  free(request_json);

  if (!success) {
    return false;
  }

  // Extract DSL from response
  if (!ExtractDSLFromResponse(response.Get(), response.GetLength(), out_dsl, error_msg)) {
    return false;
  }

  // Log success
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA OpenAI: Generated DSL (%d chars): %.100s%s\n",
               out_dsl.GetLength(), out_dsl.Get(), out_dsl.GetLength() > 100 ? "..." : "");
      ShowConsoleMsg(msg);
    }
  }

  return true;
}

bool MagdaOpenAI::GenerateDSLStream(const char *question, const char *system_prompt,
                                    const char *state_json, StreamCallback callback,
                                    WDL_FastString &error_msg) {
  // For now, use non-streaming implementation and call callback once at end
  // TODO: Implement true streaming with SSE parsing

  WDL_FastString dsl;
  bool success = GenerateDSLWithState(question, system_prompt, state_json, dsl, error_msg);

  if (success && callback) {
    callback(dsl.Get(), true);
  }

  return success;
}

// ============================================================================
// API Key Validation
// ============================================================================

#ifdef _WIN32
bool MagdaOpenAI::ValidateAPIKey(WDL_FastString &error_msg) {
  if (!HasAPIKey()) {
    error_msg.Set("API key not set");
    return false;
  }

  HINTERNET hSession = nullptr;
  HINTERNET hConnect = nullptr;
  HINTERNET hRequest = nullptr;
  bool success = false;

  hSession = WinHttpOpen(L"MAGDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                         WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    error_msg.Set("Failed to initialize WinHTTP");
    return false;
  }

  WinHttpSetTimeouts(hSession, 10000, 10000, 10000, 10000);

  hConnect = WinHttpConnect(hSession, L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    error_msg.Set("Failed to connect");
    WinHttpCloseHandle(hSession);
    return false;
  }

  // GET request to /v1/models
  hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/v1/models", nullptr, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    error_msg.Set("Failed to create request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Set Authorization header
  wchar_t headers[512];
  swprintf(headers, 512, L"Authorization: Bearer %S", m_api_key.Get());
  WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
                          0)) {
    error_msg.Set("Failed to send request");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    error_msg.Set("Failed to receive response");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                      WINHTTP_NO_HEADER_INDEX);

  if (statusCode == 200) {
    success = true;
  } else if (statusCode == 401) {
    error_msg.Set("Invalid API key");
  } else {
    error_msg.SetFormatted(256, "HTTP error %lu", statusCode);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return success;
}

#else
// macOS/Linux curl implementation
bool MagdaOpenAI::ValidateAPIKey(WDL_FastString &error_msg) {
  if (!HasAPIKey()) {
    error_msg.Set("API key not set");
    return false;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    return false;
  }

  WDL_FastString response;
  CurlWriteData writeData;
  writeData.response = &response;

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/models");
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

  // Set Authorization header
  struct curl_slist *headers = nullptr;
  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_api_key.Get());
  headers = curl_slist_append(headers, auth_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  bool success = false;
  if (res == CURLE_OK) {
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 200) {
      success = true;
    } else if (response_code == 401) {
      error_msg.Set("Invalid API key");
    } else {
      error_msg.SetFormatted(256, "HTTP error %ld", response_code);
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return success;
}
#endif

// ============================================================================
// Mix Analysis (Free-form text, no CFG constraints)
// ============================================================================

// Mix analysis system prompt
static const char *MIX_ANALYSIS_SYSTEM_PROMPT =
    R"(You are MAGDA, an expert audio mixing engineer AI assistant integrated into REAPER DAW.

You have received spectral analysis, dynamics data, and track information. Analyze the audio and provide:

1. **Frequency Balance**: Comment on the overall tonal balance (bass, mids, highs)
2. **Dynamics**: Evaluate the dynamic range and compression characteristics
3. **Mix Issues**: Identify any problems (muddiness, harshness, masking, etc.)
4. **Recommendations**: Suggest specific EQ, compression, or other processing
5. **Plugin Suggestions**: If helpful, recommend specific plugins or settings

Be concise but thorough. Focus on actionable advice. Use proper audio engineering terminology.

Format your response in clear sections with markdown headers.)";

bool MagdaOpenAI::GenerateMixFeedback(const char *analysis_json, const char *track_context_json,
                                      const char *user_request, StreamCallback callback,
                                      WDL_FastString &error_msg) {
  if (!HasAPIKey()) {
    error_msg.Set("OpenAI API key not configured");
    return false;
  }

  // Build request JSON for simple chat completion (no CFG tools)
  WDL_FastString json;
  WDL_FastString escaped;

  json.Set("{");

  // Model - use gpt-4.1 for analysis
  json.Append("\"model\":\"gpt-4.1\",");

  // Enable streaming for real-time text output
  json.Append("\"stream\":true,");

  // Input messages
  json.Append("\"input\":[");

  // Add analysis data
  json.Append("{\"role\":\"user\",\"content\":\"Audio Analysis Data:\\n");
  EscapeJSONString(analysis_json ? analysis_json : "{}", escaped);
  json.Append(escaped.Get());
  json.Append("\"}");

  // Add track context
  if (track_context_json && *track_context_json) {
    json.Append(",{\"role\":\"user\",\"content\":\"Track Context:\\n");
    EscapeJSONString(track_context_json, escaped);
    json.Append(escaped.Get());
    json.Append("\"}");
  }

  // Add user request
  if (user_request && *user_request) {
    json.Append(",{\"role\":\"user\",\"content\":\"User Request: ");
    EscapeJSONString(user_request, escaped);
    json.Append(escaped.Get());
    json.Append("\"}");
  }

  json.Append("],");

  // System prompt (instructions)
  json.Append("\"instructions\":\"");
  EscapeJSONString(MIX_ANALYSIS_SYSTEM_PROMPT, escaped);
  json.Append(escaped.Get());
  json.Append("\",");

  // Plain text output (no tools, no grammar)
  json.Append("\"text\":{\"format\":{\"type\":\"text\"}}");

  json.Append("}");

  // Log request
  void (*ShowConsoleMsg)(const char *) = nullptr;
  if (g_rec) {
    ShowConsoleMsg = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA OpenAI: Sending streaming mix analysis request...\n");
    }
  }

#ifndef _WIN32
  // macOS/Linux: Use curl with streaming
  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    return false;
  }

  CurlStreamWriteData streamData;
  streamData.callback = callback;
  streamData.line_pos = 0;
  streamData.success = false;
  streamData.received_content = false;

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/responses");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.Get());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.GetLength());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStreamWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamData);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutes for streaming
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  // Set headers
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: text/event-stream");

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_api_key.Get());
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA OpenAI: Starting SSE stream...\n");
  }

  CURLcode res = curl_easy_perform(curl);

  bool success = false;
  if (res == CURLE_OK) {
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Success if: HTTP 200 AND (explicit done event OR received content)
    if (response_code == 200 && (streamData.success || streamData.received_content)) {
      success = true;
      // Call callback with is_done=true if we haven't already
      if (!streamData.success && callback) {
        callback("", true);
      }
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA OpenAI: Streaming complete\n");
      }
    } else if (response_code != 200) {
      error_msg.SetFormatted(512, "HTTP %ld: %s", response_code,
                             streamData.error_msg.GetLength() > 0 ? streamData.error_msg.Get()
                                                                  : "Unknown error");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA OpenAI: Streaming failed: %s\n", error_msg.Get());
        ShowConsoleMsg(msg);
      }
    } else {
      // HTTP 200 but no content received
      error_msg.Set("No content received from stream");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA OpenAI: No content received from stream\n");
      }
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA OpenAI: Curl error: %s\n", error_msg.Get());
      ShowConsoleMsg(msg);
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return success;

#else
  // Windows: Fall back to non-streaming for now
  // TODO: Implement WinHTTP streaming
  WDL_FastString response;
  bool success = SendHTTPSRequest("https://api.openai.com/v1/responses", json.Get(),
                                  json.GetLength(), response, error_msg);

  if (!success) {
    return false;
  }

  // Extract text from response
  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(response.Get(), response.GetLength());

  if (parser.m_err || !root) {
    error_msg.SetFormatted(256, "Failed to parse API response: %s",
                           parser.m_err ? parser.m_err : "unknown error");
    return false;
  }

  // Check for API error
  wdl_json_element *error_elem = root->get_item_by_name("error");
  if (error_elem) {
    wdl_json_element *msg_elem = error_elem->get_item_by_name("message");
    if (msg_elem && msg_elem->m_value_string && msg_elem->m_value) {
      error_msg.SetFormatted(512, "OpenAI API error: %s", msg_elem->m_value);
      return false;
    }
  }

  // Navigate to output array and extract text
  wdl_json_element *output = root->get_item_by_name("output");
  if (!output || !output->is_array()) {
    error_msg.Set("Response missing 'output' array");
    return false;
  }

  // Find message content
  WDL_FastString full_text;
  int output_count = output->m_array ? output->m_array->GetSize() : 0;
  for (int i = 0; i < output_count; i++) {
    wdl_json_element *item = output->enum_item(i);
    if (!item)
      continue;

    wdl_json_element *type_elem = item->get_item_by_name("type");
    if (!type_elem || !type_elem->m_value_string)
      continue;

    if (strcmp(type_elem->m_value, "message") == 0) {
      wdl_json_element *content = item->get_item_by_name("content");
      if (content && content->is_array()) {
        int content_count = content->m_array ? content->m_array->GetSize() : 0;
        for (int j = 0; j < content_count; j++) {
          wdl_json_element *content_item = content->enum_item(j);
          if (!content_item)
            continue;

          wdl_json_element *text_elem = content_item->get_item_by_name("text");
          if (text_elem && text_elem->m_value_string && text_elem->m_value) {
            full_text.Append(text_elem->m_value);
          }
        }
      }
    }
  }

  if (full_text.GetLength() == 0) {
    error_msg.Set("No text output found in API response");
    return false;
  }

  // Call callback with full text
  if (callback) {
    callback(full_text.Get(), true);
  }

  return true;
#endif
}

// ============================================================================
// JSFX Generation with Streaming
// ============================================================================

bool MagdaOpenAI::GenerateJSFXStream(const char *question, const char *existing_code,
                                     StreamCallback callback, WDL_FastString &error_msg) {
  if (!HasAPIKey()) {
    error_msg.Set("OpenAI API key not configured");
    return false;
  }

  // Build request JSON - use plain text output (no CFG grammar for streaming)
  // CFG grammar requires special SDK support; plain text with strong prompt
  // works well
  WDL_FastString json;
  WDL_FastString escaped;

  json.Set("{");

  // Model - use gpt-4.1 for JSFX
  json.Append("\"model\":\"gpt-4.1\",");

  // Enable streaming
  json.Append("\"stream\":true,");

  // Input messages
  json.Append("\"input\":[");

  // Add existing code context if provided
  if (existing_code && *existing_code) {
    json.Append("{\"role\":\"user\",\"content\":\"Current JSFX code:\\n```\\n");
    EscapeJSONString(existing_code, escaped);
    json.Append(escaped.Get());
    json.Append("\\n```\"},");
  }

  // Add user question
  json.Append("{\"role\":\"user\",\"content\":\"");
  EscapeJSONString(question, escaped);
  json.Append(escaped.Get());
  json.Append("\"}");

  json.Append("],");

  // Use the comprehensive system prompt from JSFX grammar header
  json.Append("\"instructions\":\"");
  EscapeJSONString(JSFX_SYSTEM_PROMPT, escaped);
  json.Append(escaped.Get());
  json.Append("\",");

  // Plain text output format (for streaming)
  json.Append("\"text\":{\"format\":{\"type\":\"text\"}}");

  json.Append("}");

  // Log request
  void (*ShowConsoleMsg)(const char *) = nullptr;
  if (g_rec) {
    ShowConsoleMsg = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA OpenAI: Sending streaming JSFX generation request...\n");
    }
  }

#ifndef _WIN32
  // macOS/Linux: Use curl with streaming
  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg.Set("Failed to initialize curl");
    return false;
  }

  CurlStreamWriteData streamData;
  streamData.callback = callback;
  streamData.line_pos = 0;
  streamData.success = false;
  streamData.received_content = false;

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/responses");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.Get());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.GetLength());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStreamWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamData);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutes for CFG
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);

  // Set headers
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: text/event-stream");

  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", m_api_key.Get());
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA OpenAI: Starting JSFX SSE stream...\n");
  }

  CURLcode res = curl_easy_perform(curl);

  bool success = false;
  if (res == CURLE_OK) {
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 200 && (streamData.success || streamData.received_content)) {
      success = true;
      if (streamData.received_content && !streamData.success && callback) {
        callback("", true);
      }
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA OpenAI: JSFX streaming complete\n");
      }
    } else if (response_code != 200) {
      error_msg.SetFormatted(512, "HTTP %ld: %s", response_code,
                             streamData.error_msg.GetLength() > 0 ? streamData.error_msg.Get()
                                                                  : "API error");
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA OpenAI: JSFX streaming failed: %s\n", error_msg.Get());
        ShowConsoleMsg(msg);
      }
    } else {
      error_msg.Set("No JSFX code received from API");
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA OpenAI: No JSFX code received\n");
      }
    }
  } else {
    error_msg.Set(curl_easy_strerror(res));
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA OpenAI: JSFX curl error: %s\n", error_msg.Get());
      ShowConsoleMsg(msg);
    }
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return success;

#else
  // Windows: Fall back to non-streaming
  error_msg.Set("JSFX streaming not yet implemented for Windows");
  return false;
#endif
}
