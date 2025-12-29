#ifndef MAGDA_OPENAI_H
#define MAGDA_OPENAI_H

#include "../WDL/WDL/wdlstring.h"
#include <functional>

// ============================================================================
// Direct OpenAI API Client
// ============================================================================
// Makes calls to OpenAI API with CFG grammar for constrained DSL output.
// This eliminates the need for the Go API middleware for basic operations.
//
// Flow:
//   User question → OpenAI (with CFG grammar) → DSL code → DSL Interpreter → REAPER
// ============================================================================

class MagdaOpenAI {
public:
    MagdaOpenAI();
    ~MagdaOpenAI();

    // Set API key (required before making requests)
    void SetAPIKey(const char* api_key);

    // Set model (default: gpt-4.1)
    void SetModel(const char* model);

    // Generate DSL code from user question
    // Returns true on success, DSL code in out_dsl
    bool GenerateDSL(const char* question,
                     const char* system_prompt,
                     WDL_FastString& out_dsl,
                     WDL_FastString& error_msg);

    // Generate DSL code with REAPER state context
    bool GenerateDSLWithState(const char* question,
                              const char* system_prompt,
                              const char* state_json,
                              WDL_FastString& out_dsl,
                              WDL_FastString& error_msg);

    // Streaming callback: called with partial DSL as it arrives
    // Return false from callback to cancel stream
    using StreamCallback = std::function<bool(const char* partial_dsl, bool is_done)>;

    // Generate DSL with streaming (for UI updates)
    bool GenerateDSLStream(const char* question,
                           const char* system_prompt,
                           const char* state_json,
                           StreamCallback callback,
                           WDL_FastString& error_msg);

    // Generate free-form text response (for Mix Analysis, no grammar constraints)
    // Uses streaming to return text as it arrives
    bool GenerateMixFeedback(const char* analysis_json,
                             const char* track_context_json,
                             const char* user_request,
                             StreamCallback callback,
                             WDL_FastString& error_msg);

    // Generate JSFX code with streaming (uses CFG grammar for structure)
    // Streams characters as they arrive from the LLM
    bool GenerateJSFXStream(const char* question,
                            const char* existing_code,
                            StreamCallback callback,
                            WDL_FastString& error_msg);

    // Check if API key is configured
    bool HasAPIKey() const { return m_api_key.GetLength() > 0; }

    // Validate API key by making a simple API call (GET /v1/models)
    bool ValidateAPIKey(WDL_FastString& error_msg);

    // Get/set timeout (seconds)
    void SetTimeout(int seconds) { m_timeout_seconds = seconds; }
    int GetTimeout() const { return m_timeout_seconds; }

private:
    // Build request JSON with CFG grammar tool
    char* BuildRequestJSON(const char* question,
                           const char* system_prompt,
                           const char* state_json);

    // Extract DSL from response JSON
    bool ExtractDSLFromResponse(const char* response_json,
                                int response_len,
                                WDL_FastString& out_dsl,
                                WDL_FastString& error_msg);

    // Make HTTPS POST request
    bool SendHTTPSRequest(const char* url,
                          const char* post_data,
                          int post_data_len,
                          WDL_FastString& response,
                          WDL_FastString& error_msg);

    WDL_FastString m_api_key;
    WDL_FastString m_model;
    int m_timeout_seconds;
};

// ============================================================================
// Global OpenAI client instance
// ============================================================================
MagdaOpenAI* GetMagdaOpenAI();

#endif // MAGDA_OPENAI_H
