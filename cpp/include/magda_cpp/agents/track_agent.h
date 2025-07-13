#pragma once

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/models.h"
#include <map>

namespace magda {

/**
 * @brief Agent responsible for handling track creation operations
 * 
 * This agent can create audio and MIDI tracks with various VST plugins
 * and instruments. It uses LLM to parse natural language requests and
 * generate appropriate DAW commands.
 */
class TrackAgent : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (optional, will use environment variable if not provided)
     */
    TrackAgent(const std::string& api_key = "");

    /**
     * @brief Check if this agent can handle track operations
     * @param operation The operation string to check
     * @return true if the agent can handle this operation
     */
    bool canHandle(const std::string& operation) const override;

    /**
     * @brief Execute track creation operation
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
     * @brief Get track information by ID
     * @param track_id The track ID to look up
     * @return Track information as JSON, or empty JSON if not found
     */
    nlohmann::json getTrackById(const std::string& track_id) const;

    /**
     * @brief List all created tracks
     * @return Vector of track information
     */
    std::vector<nlohmann::json> listTracks() const;

private:
    std::map<std::string, nlohmann::json> created_tracks_;

    /**
     * @brief Parse track operation with LLM
     * @param operation The operation string
     * @return Parsed track information as JSON
     */
    nlohmann::json parseTrackOperationWithLLM(const std::string& operation);

    /**
     * @brief Generate DAW command from track result
     * @param result The track result to convert
     * @return DAW command string
     */
    std::string generateDAWCommand(const nlohmann::json& result) const override;
};

} // namespace magda 