#ifndef MAGDA_AGENTS_H
#define MAGDA_AGENTS_H

#include "../WDL/WDL/wdlstring.h"
#include <string>
#include <functional>

// ============================================================================
// Agent Types
// ============================================================================
enum class AgentType {
    DAW,        // Track/clip/FX operations (always runs)
    Arranger,   // Melodic/harmonic MIDI content
    Drummer,    // Drum patterns
    JSFX        // JSFX effect generation
};

// ============================================================================
// Agent Detection Result
// ============================================================================
struct AgentDetection {
    bool needsDAW = true;       // Always true
    bool needsArranger = false;
    bool needsDrummer = false;
    bool needsJSFX = false;
};

// ============================================================================
// Agent Result
// ============================================================================
struct AgentResult {
    bool success = false;
    std::string dslCode;
    std::string error;
    AgentType agentType;
};

// ============================================================================
// MagdaAgentManager - Routes requests to appropriate agents
// ============================================================================
class MagdaAgentManager {
public:
    MagdaAgentManager();
    ~MagdaAgentManager();

    // Set API key (required)
    void SetAPIKey(const char* api_key);
    bool HasAPIKey() const;

    // Detect which agents are needed for a question (uses gpt-4.1-mini)
    bool DetectAgents(const char* question, AgentDetection& result, WDL_FastString& error);

    // Generate DSL using specific agent
    bool GenerateDAW(const char* question, const char* state_json,
                     WDL_FastString& out_dsl, WDL_FastString& error);

    bool GenerateArranger(const char* question,
                          WDL_FastString& out_dsl, WDL_FastString& error);

    bool GenerateDrummer(const char* question,
                         WDL_FastString& out_dsl, WDL_FastString& error);

    bool GenerateJSFX(const char* question, const char* existing_code,
                      WDL_FastString& out_code, WDL_FastString& error);

    // Orchestrate: detect agents, run in parallel, merge results
    bool Orchestrate(const char* question, const char* state_json,
                     std::vector<AgentResult>& results, WDL_FastString& error);

private:
    // Internal helper to call OpenAI with CFG grammar
    bool CallWithCFG(const char* model,
                     const char* question,
                     const char* system_prompt,
                     const char* tool_name,
                     const char* tool_description,
                     const char* grammar,
                     WDL_FastString& out_dsl,
                     WDL_FastString& error);

    // Build request JSON for specific agent
    char* BuildAgentRequest(const char* model,
                            const char* question,
                            const char* system_prompt,
                            const char* tool_name,
                            const char* tool_description,
                            const char* grammar);

    // HTTP request
    bool SendHTTPSRequest(const char* url, const char* post_data, int post_data_len,
                          WDL_FastString& response, WDL_FastString& error);

    // Extract DSL from response
    bool ExtractDSL(const char* response_json, int len, const char* tool_name,
                    WDL_FastString& out_dsl, WDL_FastString& error);

    WDL_FastString m_api_key;
    int m_timeout_seconds;
};

// ============================================================================
// Global instance
// ============================================================================
MagdaAgentManager* GetMagdaAgentManager();

#endif // MAGDA_AGENTS_H
