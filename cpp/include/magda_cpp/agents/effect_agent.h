#pragma once

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/models.h"
#include <llmcpp/openai/OpenAIClient.h>
#include <memory>
#include <string>
#include <map>

namespace magda_cpp {

/**
 * @brief Agent responsible for handling effect operations using LLM
 *
 * Processes effect-related operations like adding reverb, delay, compressor, etc.
 * to tracks in the DAW system.
 */
class EffectAgent : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (can be empty to use environment variable)
     */
    explicit EffectAgent(const std::string& api_key = "");

    /**
     * @brief Check if this agent can handle the given operation
     * @param operation The operation string
     * @return True if this agent can handle the operation
     */
    bool canHandle(const std::string& operation) const override;

    /**
     * @brief Execute the effect operation
     * @param operation The operation description
     * @param context Optional context information
     * @return AgentResponse with DAW command and result
     */
    AgentResponse execute(const std::string& operation,
                         const nlohmann::json& context = nlohmann::json::object()) override;

    /**
     * @brief Get list of operations this agent can handle
     * @return Vector of supported operation types
     */
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief Get effect by ID
     * @param effect_id The effect ID
     * @return Effect result if found, nullopt otherwise
     */
    std::optional<EffectResult> getEffectById(const std::string& effect_id) const;

    /**
     * @brief List all created effects
     * @return Vector of all effect results
     */
    std::vector<EffectResult> listEffects() const;

private:
    std::unique_ptr<llmcpp::OpenAI::OpenAIClient> client_;
    std::map<std::string, EffectResult> effects_;

    /**
     * @brief Parse effect operation using LLM
     * @param operation The operation string
     * @return Parsed effect information
     */
    nlohmann::json parseEffectOperationWithLLM(const std::string& operation);

    /**
     * @brief Generate DAW command from effect result
     * @param effect The effect result
     * @return DAW command string
     */
    std::string generateDawCommand(const EffectResult& effect) const;

    /**
     * @brief Get track ID from context
     * @param context The context object
     * @return Track ID string
     */
    std::string getTrackIdFromContext(const nlohmann::json& context) const;
};

} // namespace magda_cpp
