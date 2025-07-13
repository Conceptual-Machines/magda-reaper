#pragma once

#include "magda_cpp/agents/base_agent.h"
#include <openai/OpenAIClient.h>
#include <core/JsonSchemaBuilder.h>
#include <core/LLMTypes.h>
#include <string>
#include <vector>
#include <memory>

namespace magda {

/**
 * @brief Represents a single DAW operation identified from a prompt
 */
struct DAWOperation {
    std::string type;           // "track", "clip", "volume", "effect", "midi"
    std::string description;    // Human-readable description
    nlohmann::json parameters;  // Operation-specific parameters
};

/**
 * @brief Result of operation identification
 */
struct OperationIdentificationResult {
    std::vector<DAWOperation> operations;
    std::string original_prompt;
    bool success;
    std::string error_message;
};

/**
 * @brief Agent responsible for identifying DAW operations from natural language prompts
 *
 * Uses OpenAI's GPT models via llmcpp to analyze prompts and extract structured
 * DAW operations that can be executed by specialized agents.
 */
class OperationIdentifier : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (can be nullptr to use environment variable)
     */
    explicit OperationIdentifier(const std::string& api_key = "");

    // BaseAgent interface implementation
    bool canHandle(const std::string& operation) const override;
    AgentResponse execute(const std::string& operation, const nlohmann::json& context = nlohmann::json::object()) override;
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief Identify operations in a natural language prompt
     * @param prompt The natural language prompt to analyze
     * @return Result containing identified operations
     */
    OperationIdentificationResult identifyOperations(const std::string& prompt);

    /**
     * @brief Get the recommended model for operation identification
     * @return Model string for optimal performance/cost balance
     */
    static std::string getRecommendedModel();

    /**
     * @brief Get the JSON schema for DAW operations
     * @return JSON schema for structured output
     */
    static nlohmann::json getOperationSchema();

private:
    // BaseAgent pure virtual method implementation
    std::string generateDAWCommand(const nlohmann::json& result) const override;

    /**
     * @brief Build the system prompt for operation identification
     * @return System prompt string
     */
    static std::string buildSystemPrompt();

    /**
     * @brief Check if structured output should be used based on model
     * @return true if structured output should be used
     */
    bool shouldUseStructuredOutput();

    /**
     * @brief Parse structured LLM response (with schema)
     * @param response LLM response
     * @return Parsed operations
     */
    std::vector<DAWOperation> parseStructuredOperations(const LLMResponse& response);

    /**
     * @brief Parse free-form LLM response (without schema)
     * @param response LLM response
     * @return Parsed operations
     */
    std::vector<DAWOperation> parseFreeFormOperations(const LLMResponse& response);
};

} // namespace magda
