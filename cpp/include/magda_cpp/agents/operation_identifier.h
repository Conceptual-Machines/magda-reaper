#pragma once

#include <llmcpp/openai/OpenAIClient.h>
#include <llmcpp/core/JsonSchemaBuilder.h>
#include <llmcpp/core/LLMRequest.h>
#include <string>
#include <vector>
#include <memory>

namespace magda_cpp {

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
class OperationIdentifier {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (can be nullptr to use environment variable)
     */
    explicit OperationIdentifier(const std::string& api_key = "");

    /**
     * @brief Identify operations in a natural language prompt
     * @param prompt The natural language prompt to analyze
     * @return Result containing identified operations
     */
    OperationIdentificationResult identifyOperations(const std::string& prompt);

    /**
     * @brief Get the recommended model for operation identification
     * @return Model enum for optimal performance/cost balance
     */
    static llmcpp::OpenAI::Model getRecommendedModel();

    /**
     * @brief Get the JSON schema for DAW operations
     * @return JSON schema for structured output
     */
    static nlohmann::json getOperationSchema();

private:
    std::unique_ptr<llmcpp::OpenAIClient> client_;

    /**
     * @brief Build the system prompt for operation identification
     * @return System prompt string
     */
    static std::string buildSystemPrompt();

    /**
     * @brief Parse the LLM response into DAW operations
     * @param response LLM response
     * @return Parsed operations
     */
    std::vector<DAWOperation> parseOperations(const llmcpp::LLMResponse& response);
};

} // namespace magda_cpp
