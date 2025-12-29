#include "magda_openai.h"
#include "../dsl/magda_dsl_grammar.h"
#include "reaper_plugin.h"
#include "../WDL/WDL/jsonparse.h"
#include <cstring>
#include <cstdlib>

extern reaper_plugin_info_t* g_rec;

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

static MagdaOpenAI* s_openai_instance = nullptr;

MagdaOpenAI* GetMagdaOpenAI() {
    if (!s_openai_instance) {
        s_openai_instance = new MagdaOpenAI();
    }
    return s_openai_instance;
}

// ============================================================================
// MagdaOpenAI Implementation
// ============================================================================

MagdaOpenAI::MagdaOpenAI()
    : m_timeout_seconds(60)
{
    m_model.Set("gpt-5.1");  // GPT-5.1 for DSL generation (CFG)
}

MagdaOpenAI::~MagdaOpenAI() {
}

void MagdaOpenAI::SetAPIKey(const char* api_key) {
    if (api_key) {
        m_api_key.Set(api_key);
    }
}

void MagdaOpenAI::SetModel(const char* model) {
    if (model) {
        m_model.Set(model);
    }
}

// Helper to escape JSON string
static void EscapeJSONString(const char* input, WDL_FastString& output) {
    output.Set("");
    while (input && *input) {
        switch (*input) {
            case '"':  output.Append("\\\""); break;
            case '\\': output.Append("\\\\"); break;
            case '\n': output.Append("\\n"); break;
            case '\r': output.Append("\\r"); break;
            case '\t': output.Append("\\t"); break;
            default:   output.Append(input, 1); break;
        }
        input++;
    }
}

char* MagdaOpenAI::BuildRequestJSON(const char* question,
                                     const char* system_prompt,
                                     const char* state_json) {
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
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, json.Get(), len);
        result[len] = '\0';
    }

    return result;
}

bool MagdaOpenAI::ExtractDSLFromResponse(const char* response_json,
                                          int response_len,
                                          WDL_FastString& out_dsl,
                                          WDL_FastString& error_msg) {
    if (!response_json || response_len <= 0) {
        error_msg.Set("Empty response from API");
        return false;
    }

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(response_json, response_len);

    if (parser.m_err || !root) {
        error_msg.SetFormatted(256, "Failed to parse API response: %s",
                              parser.m_err ? parser.m_err : "unknown error");
        return false;
    }

    // Check for API error
    wdl_json_element* error_elem = root->get_item_by_name("error");
    if (error_elem) {
        wdl_json_element* msg_elem = error_elem->get_item_by_name("message");
        if (msg_elem && msg_elem->m_value_string) {
            error_msg.SetFormatted(512, "OpenAI API error: %s", msg_elem->m_value);
        } else {
            error_msg.Set("OpenAI API returned an error");
        }
        return false;
    }

    // Navigate to output array
    wdl_json_element* output = root->get_item_by_name("output");
    if (!output || !output->is_array()) {
        error_msg.Set("Response missing 'output' array");
        return false;
    }

    // Search for custom_tool_call in output items (CFG grammar response)
    int output_count = output->m_array ? output->m_array->GetSize() : 0;
    for (int i = 0; i < output_count; i++) {
        wdl_json_element* item = output->enum_item(i);
        if (!item) continue;

        wdl_json_element* type_elem = item->get_item_by_name("type");
        if (!type_elem || !type_elem->m_value_string) continue;

        // CFG tool calls have type "custom_tool_call"
        if (strcmp(type_elem->m_value, "custom_tool_call") == 0) {
            // Check if this is our magda_dsl tool
            wdl_json_element* name_elem = item->get_item_by_name("name");
            if (name_elem && name_elem->m_value_string &&
                strcmp(name_elem->m_value, "magda_dsl") == 0) {
                // DSL code is directly in the "input" field (raw text, not JSON)
                wdl_json_element* input_elem = item->get_item_by_name("input");
                if (input_elem && input_elem->m_value_string && input_elem->m_value[0]) {
                    out_dsl.Set(input_elem->m_value);
                    return true;
                }
            }
        }
    }

    // Fallback: check for text output (in case CFG wasn't used)
    for (int i = 0; i < output_count; i++) {
        wdl_json_element* item = output->enum_item(i);
        if (!item) continue;

        wdl_json_element* type_elem = item->get_item_by_name("type");
        if (!type_elem || !type_elem->m_value_string) continue;

        if (strcmp(type_elem->m_value, "message") == 0) {
            wdl_json_element* content = item->get_item_by_name("content");
            if (content && content->is_array()) {
                int content_count = content->m_array ? content->m_array->GetSize() : 0;
                for (int j = 0; j < content_count; j++) {
                    wdl_json_element* content_item = content->enum_item(j);
                    if (!content_item) continue;

                    wdl_json_element* text_elem = content_item->get_item_by_name("text");
                    if (text_elem && text_elem->m_value_string) {
                        // Check if this looks like DSL
                        const char* text = text_elem->m_value;
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
bool MagdaOpenAI::SendHTTPSRequest(const char* url,
                                    const char* post_data,
                                    int post_data_len,
                                    WDL_FastString& response,
                                    WDL_FastString& error_msg) {
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

    hSession = WinHttpOpen(L"MAGDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error_msg.Set("Failed to initialize WinHTTP");
        return false;
    }

    // Set timeout
    DWORD timeout_ms = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession, 30000, 30000, timeout_ms, timeout_ms);

    // Convert hostname to wide string
    int hostname_len = MultiByteToWideChar(CP_UTF8, 0, hostname, -1, nullptr, 0);
    wchar_t* hostname_w = new wchar_t[hostname_len];
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
    wchar_t* path_w = new wchar_t[path_len];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, path_len);

    hRequest = WinHttpOpenRequest(hConnect, L"POST", path_w, nullptr,
                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE);
    delete[] path_w;

    if (!hRequest) {
        error_msg.Set("Failed to create request");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set headers
    wchar_t headers[1024];
    swprintf(headers, 1024,
             L"Content-Type: application/json\r\nAuthorization: Bearer %S\r\n",
             m_api_key.Get());
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

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

    // Check status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);

    // Read response body
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        char* buffer = (char*)malloc(bytesAvailable + 1);
        if (!buffer) break;

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
    WDL_FastString* response;
};

static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    CurlWriteData* data = (CurlWriteData*)userp;
    if (data->response) {
        data->response->Append((const char*)contents, (int)realsize);
    }
    return realsize;
}

bool MagdaOpenAI::SendHTTPSRequest(const char* url,
                                    const char* post_data,
                                    int post_data_len,
                                    WDL_FastString& response,
                                    WDL_FastString& error_msg) {
    CURL* curl = curl_easy_init();
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
    struct curl_slist* headers = nullptr;
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

bool MagdaOpenAI::GenerateDSL(const char* question,
                               const char* system_prompt,
                               WDL_FastString& out_dsl,
                               WDL_FastString& error_msg) {
    return GenerateDSLWithState(question, system_prompt, nullptr, out_dsl, error_msg);
}

bool MagdaOpenAI::GenerateDSLWithState(const char* question,
                                        const char* system_prompt,
                                        const char* state_json,
                                        WDL_FastString& out_dsl,
                                        WDL_FastString& error_msg) {
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
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[512];
            snprintf(msg, sizeof(msg), "MAGDA OpenAI: Generating DSL for: %.100s%s\n",
                    question, strlen(question) > 100 ? "..." : "");
            ShowConsoleMsg(msg);
        }
    }

    // Build request JSON
    char* request_json = BuildRequestJSON(question, system_prompt, state_json);
    if (!request_json) {
        error_msg.Set("Failed to build request JSON");
        return false;
    }

    // Log request for debugging (truncated)
    if (g_rec) {
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[2048];
            snprintf(msg, sizeof(msg), "MAGDA OpenAI: Request JSON (first 1000 chars): %.1000s%s\n",
                    request_json, strlen(request_json) > 1000 ? "..." : "");
            ShowConsoleMsg(msg);
        }
    }

    // Make API request
    WDL_FastString response;
    bool success = SendHTTPSRequest("https://api.openai.com/v1/responses",
                                    request_json, (int)strlen(request_json),
                                    response, error_msg);
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
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[512];
            snprintf(msg, sizeof(msg), "MAGDA OpenAI: Generated DSL (%d chars): %.100s%s\n",
                    out_dsl.GetLength(), out_dsl.Get(),
                    out_dsl.GetLength() > 100 ? "..." : "");
            ShowConsoleMsg(msg);
        }
    }

    return true;
}

bool MagdaOpenAI::GenerateDSLStream(const char* question,
                                     const char* system_prompt,
                                     const char* state_json,
                                     StreamCallback callback,
                                     WDL_FastString& error_msg) {
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
bool MagdaOpenAI::ValidateAPIKey(WDL_FastString& error_msg) {
    if (!HasAPIKey()) {
        error_msg.Set("API key not set");
        return false;
    }

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;

    hSession = WinHttpOpen(L"MAGDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
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
    hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/v1/models",
                                  nullptr, WINHTTP_NO_REFERER,
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

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
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
bool MagdaOpenAI::ValidateAPIKey(WDL_FastString& error_msg) {
    if (!HasAPIKey()) {
        error_msg.Set("API key not set");
        return false;
    }

    CURL* curl = curl_easy_init();
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
    struct curl_slist* headers = nullptr;
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
