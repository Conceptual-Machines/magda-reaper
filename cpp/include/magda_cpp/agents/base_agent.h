#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <llmcpp.h>
#include <llmcpp/core/JsonSchemaBuilder.h>

namespace magda {

/**
 * @brief Base class for all MAGDA agents
 * 
 * This class provides the common interface and functionality that all agents
 * must implement. It integrates with the llmcpp library for LLM operations.
 */
class BaseAgent {
public:
    /**
     * @brief Constructor
     * @param name The name of the agent
     * @param api_key The OpenAI API key
     */
    BaseAgent(const std::string& name, const std::string& api_key = "");
    
    virtual ~BaseAgent() = default;

    /**
     * @brief Check if this agent can handle the given operation
     * @param operation The operation string to check
     * @return true if the agent can handle this operation
     */
    virtual bool canHandle(const std::string& operation) const = 0;

    /**
     * @brief Execute an operation using the agent
     * @param operation The operation string to execute
     * @param context Optional context for the operation
     * @return AgentResponse containing the result
     */
    virtual AgentResponse execute(const std::string& operation, 
                                 const nlohmann::json& context = nlohmann::json::object()) = 0;

    /**
     * @brief Get the capabilities of this agent
     * @return Vector of operation types this agent can handle
     */
    virtual std::vector<std::string> getCapabilities() const = 0;

    /**
     * @brief Get the agent name
     * @return The agent name
     */
    const std::string& getName() const { return name_; }

protected:
    std::string name_;
    std::unique_ptr<llmcpp::OpenAIClient> client_;
    
    /**
     * @brief Parse operation with LLM using structured output
     * @param operation The operation string
     * @param instructions Instructions for the LLM
     * @param schema JsonSchemaBuilder for structured output
     * @return Parsed result as JSON
     */
    nlohmann::json parseOperationWithLLM(const std::string& operation,
                                         const std::string& instructions,
                                         const llmcpp::JsonSchemaBuilder& schema);

    /**
     * @brief Generate a DAW command from a result
     * @param result The result to convert
     * @return DAW command string
     */
    virtual std::string generateDAWCommand(const nlohmann::json& result) const = 0;

    /**
     * @brief Get track ID from context
     * @param context The context JSON
     * @return Track ID if found, empty string otherwise
     */
    std::string getTrackIdFromContext(const nlohmann::json& context) const;

    /**
     * @brief Get track name from context
     * @param context The context JSON
     * @return Track name if found, "unknown" otherwise
     */
    std::string getTrackNameFromContext(const nlohmann::json& context) const;

    /**
     * @brief Generate a unique ID
     * @return Unique ID string
     */
    std::string generateUniqueId() const;
};

} // namespace magda 