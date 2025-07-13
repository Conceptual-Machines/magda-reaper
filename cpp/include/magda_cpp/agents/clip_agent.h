#pragma once

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/models.h"
#include <openai/OpenAIClient.h>
#include <memory>
#include <string>
#include <map>

namespace magda {

/**
 * @brief Agent responsible for handling clip operations using LLM
 *
 * Processes clip-related operations like creating audio/MIDI clips, regions,
 * and recordings in the DAW system.
 */
class ClipAgent : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (can be empty to use environment variable)
     */
    explicit ClipAgent(const std::string& api_key = "");

    /**
     * @brief Check if this agent can handle the given operation
     * @param operation The operation string
     * @return True if this agent can handle the operation
     */
    bool canHandle(const std::string& operation) const override;

    /**
     * @brief Execute the clip operation
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
     * @brief Get clip by ID
     * @param clip_id The clip ID
     * @return Clip result if found, nullopt otherwise
     */
    std::optional<ClipResult> getClipById(const std::string& clip_id) const;

    /**
     * @brief List all created clips
     * @return Vector of all clip results
     */
    std::vector<ClipResult> listClips() const;

private:
    std::unique_ptr<OpenAIClient> client_;
    std::map<std::string, ClipResult> clips_;

    /**
     * @brief Parse clip operation using LLM
     * @param operation The operation string
     * @return Parsed clip information
     */
    nlohmann::json parseClipOperationWithLLM(const std::string& operation);

    /**
     * @brief Generate DAW command from clip result
     * @param clip The clip result
     * @return DAW command string
     */
    std::string generateDAWCommand(const nlohmann::json& result) const override;

    /**
     * @brief Get track ID from context
     * @param context The context object
     * @return Track ID string
     */
    std::string getTrackIdFromContext(const nlohmann::json& context) const;
};

} // namespace magda
