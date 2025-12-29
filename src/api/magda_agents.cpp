#include "magda_agents.h"
#include "../dsl/magda_dsl_grammar.h"
#include "../dsl/magda_arranger_grammar.h"
#include "../dsl/magda_drummer_grammar.h"
#include "../dsl/magda_jsfx_grammar.h"
#include "../WDL/WDL/jsonparse.h"
#include "reaper_plugin.h"
#include <cstring>
#include <cstdlib>
#include <thread>
#include <mutex>

extern reaper_plugin_info_t* g_rec;

#ifndef _WIN32
#include <curl/curl.h>
#endif

// ============================================================================
// Global instance
// ============================================================================
static MagdaAgentManager* s_agent_manager = nullptr;

MagdaAgentManager* GetMagdaAgentManager() {
    if (!s_agent_manager) {
        s_agent_manager = new MagdaAgentManager();
    }
    return s_agent_manager;
}

// ============================================================================
// Helper: Escape JSON string
// ============================================================================
static void EscapeJSON(const char* input, WDL_FastString& output) {
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

// ============================================================================
// MagdaAgentManager Implementation
// ============================================================================
MagdaAgentManager::MagdaAgentManager() : m_timeout_seconds(60) {}
MagdaAgentManager::~MagdaAgentManager() {}

void MagdaAgentManager::SetAPIKey(const char* api_key) {
    if (api_key) m_api_key.Set(api_key);
}

bool MagdaAgentManager::HasAPIKey() const {
    return m_api_key.GetLength() > 0;
}

// ============================================================================
// Agent Detection (gpt-4.1-mini)
// ============================================================================
bool MagdaAgentManager::DetectAgents(const char* question, AgentDetection& result, WDL_FastString& error) {
    if (!HasAPIKey()) {
        error.Set("API key not set");
        return false;
    }

    // Default: always DAW
    result.needsDAW = true;
    result.needsArranger = false;
    result.needsDrummer = false;
    result.needsJSFX = false;

    // Quick keyword detection first
    if (strstr(question, "jsfx") || strstr(question, "JSFX") ||
        strstr(question, "effect") || strstr(question, "plugin")) {
        result.needsJSFX = true;
        return true;
    }

    // Build classification prompt
    const char* classifyPrompt = R"(You are a router for a music production AI. Classify which agents are needed.

AGENTS:
1. DAW (always runs): Track operations, clips, FX, volume, pan, mute, solo
2. ARRANGER: Melodic/harmonic MIDI - chords, arpeggios, melodies, basslines, notes
3. DRUMMER: Drum/percussion patterns - kick, snare, hi-hat, beats, grooves

YOUR TASK: Return JSON with needsArranger and needsDrummer booleans.

EXAMPLES:
- "create a track" → {"needsArranger": false, "needsDrummer": false}
- "add reverb" → {"needsArranger": false, "needsDrummer": false}
- "add a chord progression in C" → {"needsArranger": true, "needsDrummer": false}
- "add E1 bass note" → {"needsArranger": true, "needsDrummer": false}
- "create a drum beat" → {"needsArranger": false, "needsDrummer": true}
- "hip hop groove with melody" → {"needsArranger": true, "needsDrummer": true}

Return ONLY JSON: {"needsArranger": bool, "needsDrummer": bool})";

    WDL_FastString json, escaped;
    json.Set("{");
    json.Append("\"model\":\"gpt-4.1-mini\",");
    json.Append("\"input\":[{\"role\":\"user\",\"content\":\"");
    EscapeJSON(question, escaped);
    json.Append(escaped.Get());
    json.Append("\"}],");
    json.Append("\"instructions\":\"");
    EscapeJSON(classifyPrompt, escaped);
    json.Append(escaped.Get());
    json.Append("\",");
    json.Append("\"reasoning\":{\"effort\":\"minimal\"},");
    json.Append("\"text\":{\"format\":{\"type\":\"json_object\"}}");
    json.Append("}");

    WDL_FastString response;
    if (!SendHTTPSRequest("https://api.openai.com/v1/responses",
                          json.Get(), json.GetLength(), response, error)) {
        // Fallback: keyword detection
        if (strstr(question, "chord") || strstr(question, "arpeggio") ||
            strstr(question, "melody") || strstr(question, "note") ||
            strstr(question, "bass")) {
            result.needsArranger = true;
        }
        if (strstr(question, "drum") || strstr(question, "beat") ||
            strstr(question, "kick") || strstr(question, "snare") ||
            strstr(question, "groove") || strstr(question, "rhythm")) {
            result.needsDrummer = true;
        }
        return true;  // Use fallback
    }

    // Parse response to extract classification
    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(response.Get(), response.GetLength());
    if (!parser.m_err && root) {
        // Navigate to output[0].content[0].text
        wdl_json_element* output = root->get_item_by_name("output");
        if (output && output->is_array() && output->m_array->GetSize() > 0) {
            wdl_json_element* item = output->enum_item(0);
            if (item) {
                wdl_json_element* content = item->get_item_by_name("content");
                if (content && content->is_array() && content->m_array->GetSize() > 0) {
                    wdl_json_element* textItem = content->enum_item(0);
                    if (textItem) {
                        wdl_json_element* textElem = textItem->get_item_by_name("text");
                        if (textElem && textElem->m_value_string) {
                            // Parse the JSON in text
                            wdl_json_parser textParser;
                            wdl_json_element* textRoot = textParser.parse(
                                textElem->m_value, (int)strlen(textElem->m_value));
                            if (!textParser.m_err && textRoot) {
                                wdl_json_element* arr = textRoot->get_item_by_name("needsArranger");
                                wdl_json_element* drum = textRoot->get_item_by_name("needsDrummer");
                                if (arr && arr->m_value) {
                                    result.needsArranger = (strcmp(arr->m_value, "true") == 0);
                                }
                                if (drum && drum->m_value) {
                                    result.needsDrummer = (strcmp(drum->m_value, "true") == 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Log detection result
    if (g_rec) {
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[256];
            snprintf(msg, sizeof(msg), "MAGDA Agent Detection: DAW=%d, Arranger=%d, Drummer=%d\n",
                     result.needsDAW, result.needsArranger, result.needsDrummer);
            ShowConsoleMsg(msg);
        }
    }

    return true;
}

// ============================================================================
// Build Agent Request JSON
// ============================================================================
char* MagdaAgentManager::BuildAgentRequest(const char* model,
                                            const char* question,
                                            const char* system_prompt,
                                            const char* tool_name,
                                            const char* tool_description,
                                            const char* grammar) {
    WDL_FastString json, escaped;

    json.Set("{");
    json.Append("\"model\":\"");
    json.Append(model);
    json.Append("\",");

    // Input
    json.Append("\"input\":[{\"role\":\"user\",\"content\":\"");
    EscapeJSON(question, escaped);
    json.Append(escaped.Get());
    json.Append("\"}],");

    // Instructions
    json.Append("\"instructions\":\"");
    EscapeJSON(system_prompt, escaped);
    json.Append(escaped.Get());
    json.Append("\",");

    // Text format
    json.Append("\"text\":{\"format\":{\"type\":\"text\"}},");

    // CFG Tool
    json.Append("\"tools\":[{");
    json.Append("\"type\":\"custom\",");
    json.Append("\"name\":\"");
    json.Append(tool_name);
    json.Append("\",");
    json.Append("\"description\":\"");
    EscapeJSON(tool_description, escaped);
    json.Append(escaped.Get());
    json.Append("\",");
    json.Append("\"format\":{");
    json.Append("\"type\":\"grammar\",");
    json.Append("\"syntax\":\"lark\",");
    json.Append("\"definition\":\"");
    EscapeJSON(grammar, escaped);
    json.Append(escaped.Get());
    json.Append("\"}}],");

    json.Append("\"parallel_tool_calls\":false");
    json.Append("}");

    char* result = (char*)malloc(json.GetLength() + 1);
    if (result) {
        memcpy(result, json.Get(), json.GetLength());
        result[json.GetLength()] = '\0';
    }
    return result;
}

// ============================================================================
// Extract DSL from Response
// ============================================================================
bool MagdaAgentManager::ExtractDSL(const char* response_json, int len, const char* tool_name,
                                    WDL_FastString& out_dsl, WDL_FastString& error) {
    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(response_json, len);

    if (parser.m_err || !root) {
        error.Set("Failed to parse response");
        return false;
    }

    // Check for error (but ignore null)
    wdl_json_element* error_elem = root->get_item_by_name("error");
    if (error_elem) {
        wdl_json_element* msg_elem = error_elem->get_item_by_name("message");
        if (msg_elem && msg_elem->m_value_string && msg_elem->m_value) {
            error.SetFormatted(512, "API error: %s", msg_elem->m_value);
            return false;
        }
    }

    // Find custom_tool_call
    wdl_json_element* output = root->get_item_by_name("output");
    if (!output || !output->is_array()) {
        error.Set("Missing output array");
        return false;
    }

    int count = output->m_array ? output->m_array->GetSize() : 0;
    for (int i = 0; i < count; i++) {
        wdl_json_element* item = output->enum_item(i);
        if (!item) continue;

        wdl_json_element* type = item->get_item_by_name("type");
        if (!type || !type->m_value_string) continue;

        if (strcmp(type->m_value, "custom_tool_call") == 0) {
            wdl_json_element* name = item->get_item_by_name("name");
            if (name && name->m_value_string && strcmp(name->m_value, tool_name) == 0) {
                wdl_json_element* input = item->get_item_by_name("input");
                if (input && input->m_value_string && input->m_value[0]) {
                    out_dsl.Set(input->m_value);
                    return true;
                }
            }
        }
    }

    error.Set("No DSL output found");
    return false;
}

// ============================================================================
// HTTP Request (curl implementation for macOS)
// ============================================================================
#ifndef _WIN32
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

bool MagdaAgentManager::SendHTTPSRequest(const char* url, const char* post_data, int post_data_len,
                                          WDL_FastString& response, WDL_FastString& error) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        error.Set("Failed to init curl");
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

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", m_api_key.Get());
    headers = curl_slist_append(headers, auth);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    bool success = false;

    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        if (code == 200) {
            success = true;
        } else {
            error.SetFormatted(512, "HTTP %ld: %.200s", code, response.Get());
        }
    } else {
        error.Set(curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}
#else
// Windows implementation would go here
bool MagdaAgentManager::SendHTTPSRequest(const char* url, const char* post_data, int post_data_len,
                                          WDL_FastString& response, WDL_FastString& error) {
    error.Set("Windows not implemented yet");
    return false;
}
#endif

// ============================================================================
// Agent Generators
// ============================================================================
bool MagdaAgentManager::GenerateDAW(const char* question, const char* state_json,
                                     WDL_FastString& out_dsl, WDL_FastString& error) {
    // Build system prompt with state
    WDL_FastString prompt;
    prompt.Set(MAGDA_DSL_TOOL_DESCRIPTION);
    if (state_json && *state_json) {
        prompt.Append("\n\nCurrent REAPER state:\n");
        prompt.Append(state_json);
    }

    char* request = BuildAgentRequest("gpt-5.1", question, prompt.Get(),
                                       "magda_dsl", MAGDA_DSL_TOOL_DESCRIPTION, MAGDA_DSL_GRAMMAR);
    if (!request) {
        error.Set("Failed to build request");
        return false;
    }

    WDL_FastString response;
    bool success = SendHTTPSRequest("https://api.openai.com/v1/responses",
                                    request, (int)strlen(request), response, error);
    free(request);

    if (!success) return false;
    return ExtractDSL(response.Get(), response.GetLength(), "magda_dsl", out_dsl, error);
}

bool MagdaAgentManager::GenerateArranger(const char* question,
                                          WDL_FastString& out_dsl, WDL_FastString& error) {
    char* request = BuildAgentRequest("gpt-5.1", question, ARRANGER_TOOL_DESCRIPTION,
                                       "arranger_dsl", ARRANGER_TOOL_DESCRIPTION, ARRANGER_DSL_GRAMMAR);
    if (!request) {
        error.Set("Failed to build request");
        return false;
    }

    WDL_FastString response;
    bool success = SendHTTPSRequest("https://api.openai.com/v1/responses",
                                    request, (int)strlen(request), response, error);
    free(request);

    if (!success) return false;
    return ExtractDSL(response.Get(), response.GetLength(), "arranger_dsl", out_dsl, error);
}

bool MagdaAgentManager::GenerateDrummer(const char* question,
                                         WDL_FastString& out_dsl, WDL_FastString& error) {
    char* request = BuildAgentRequest("gpt-5.1", question, DRUMMER_TOOL_DESCRIPTION,
                                       "drummer_dsl", DRUMMER_TOOL_DESCRIPTION, DRUMMER_DSL_GRAMMAR);
    if (!request) {
        error.Set("Failed to build request");
        return false;
    }

    WDL_FastString response;
    bool success = SendHTTPSRequest("https://api.openai.com/v1/responses",
                                    request, (int)strlen(request), response, error);
    free(request);

    if (!success) return false;
    return ExtractDSL(response.Get(), response.GetLength(), "drummer_dsl", out_dsl, error);
}

bool MagdaAgentManager::GenerateJSFX(const char* question, const char* existing_code,
                                      WDL_FastString& out_code, WDL_FastString& error) {
    WDL_FastString fullQuestion;
    fullQuestion.Set(question);
    if (existing_code && *existing_code) {
        fullQuestion.Append("\n\nExisting JSFX code:\n");
        fullQuestion.Append(existing_code);
    }

    char* request = BuildAgentRequest("gpt-5.1", fullQuestion.Get(), JSFX_SYSTEM_PROMPT,
                                       "jsfx_generator", JSFX_TOOL_DESCRIPTION, JSFX_GRAMMAR);
    if (!request) {
        error.Set("Failed to build request");
        return false;
    }

    WDL_FastString response;
    bool success = SendHTTPSRequest("https://api.openai.com/v1/responses",
                                    request, (int)strlen(request), response, error);
    free(request);

    if (!success) return false;
    return ExtractDSL(response.Get(), response.GetLength(), "jsfx_generator", out_code, error);
}

// ============================================================================
// Orchestrate - Run agents in parallel
// ============================================================================
bool MagdaAgentManager::Orchestrate(const char* question, const char* state_json,
                                     std::vector<AgentResult>& results, WDL_FastString& error) {
    // Step 1: Detect which agents are needed
    AgentDetection detection;
    if (!DetectAgents(question, detection, error)) {
        return false;
    }

    // Step 2: Run agents (DAW always runs)
    std::mutex resultsMutex;
    std::vector<std::thread> threads;

    // DAW Agent
    threads.emplace_back([&]() {
        AgentResult result;
        result.agentType = AgentType::DAW;
        WDL_FastString dsl, err;
        result.success = GenerateDAW(question, state_json, dsl, err);
        result.dslCode = dsl.Get();
        result.error = err.Get();
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.push_back(result);
    });

    // Arranger Agent
    if (detection.needsArranger) {
        threads.emplace_back([&]() {
            AgentResult result;
            result.agentType = AgentType::Arranger;
            WDL_FastString dsl, err;
            result.success = GenerateArranger(question, dsl, err);
            result.dslCode = dsl.Get();
            result.error = err.Get();
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(result);
        });
    }

    // Drummer Agent
    if (detection.needsDrummer) {
        threads.emplace_back([&]() {
            AgentResult result;
            result.agentType = AgentType::Drummer;
            WDL_FastString dsl, err;
            result.success = GenerateDrummer(question, dsl, err);
            result.dslCode = dsl.Get();
            result.error = err.Get();
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(result);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // Check if any succeeded
    bool anySuccess = false;
    for (const auto& r : results) {
        if (r.success) anySuccess = true;
    }

    if (!anySuccess && !results.empty()) {
        error.Set(results[0].error.c_str());
    }

    return anySuccess;
}
