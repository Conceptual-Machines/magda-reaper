#pragma once

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/models.h"
#include <map>

namespace magda {

/**
 * @brief Agent responsible for handling volume control operations
 *
 * This agent can control volume, pan, and mute settings for tracks.
 * It uses LLM to parse natural language requests and generate appropriate DAW commands.
 */
class VolumeAgent : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (optional, will use environment variable if not provided)
     */
    VolumeAgent(const std::string& api_key = "");

    /**
     * @brief Check if this agent can handle volume operations
     * @param operation The operation string to check
     * @return true if the agent can handle this operation
     */
    bool canHandle(const std::string& operation) const override;

    /**
     * @brief Execute volume control operation
     * @param operation The operation string to execute
     * @param context Optional context for the operation
     * @return AgentResponse containing the result
     */
    AgentResponse execute(const std::string& operation,
                         const nlohmann::json& context = nlohmann::json::object()) override;

    /**
     * @brief Get the capabilities of this agent
     * @return Vector of operation types this agent can handle
     */
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief Get volume settings for a track
     * @param track_id The track ID to look up
     * @return Volume settings as JSON, or empty JSON if not found
     */
    nlohmann::json getVolumeSettings(const std::string& track_id) const;

    /**
     * @brief List all volume settings
     * @return Vector of volume settings
     */
    std::vector<nlohmann::json> listVolumeSettings() const;

private:
    std::map<std::string, nlohmann::json> volume_settings_;

    /**
     * @brief Parse volume operation with LLM
     * @param operation The operation string
     * @return Parsed volume information as JSON
     */
    nlohmann::json parseVolumeOperationWithLLM(const std::string& operation);

    /**
     * @brief Generate DAW command from volume result
     * @param result The volume result to convert
     * @return DAW command string
     */
    std::string generateDAWCommand(const nlohmann::json& result) const override;
};

} // namespace magda
