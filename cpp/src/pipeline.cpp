#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"
#include "magda_cpp/agents/effect_agent.h"
#include "magda_cpp/agents/midi_agent.h"
#include "magda_cpp/agents/clip_agent.h"
#include "magda_cpp/agents/operation_identifier.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <iostream>

namespace magda {

/**
 * @brief Two-stage pipeline that coordinates operation identification and execution
 */
class MAGDAPipeline {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key for all agents
     */
    explicit MAGDAPipeline(const std::string& api_key = "") : api_key_(api_key) {
        // Initialize operation identifier
        operation_identifier_ = std::make_unique<magda_cpp::OperationIdentifier>(api_key);

        // Initialize specialized agents
        agents_["track"] = std::make_shared<TrackAgent>(api_key);
        agents_["clip"] = std::make_shared<ClipAgent>(api_key);
        agents_["volume"] = std::make_shared<VolumeAgent>(api_key);
        agents_["effect"] = std::make_shared<EffectAgent>(api_key);
        agents_["midi"] = std::make_shared<MidiAgent>(api_key);
    }

    /**
     * @brief Process a natural language prompt through the two-stage pipeline
     * @param prompt The natural language prompt
     * @return Pipeline result with operations and DAW commands
     */
    nlohmann::json processPrompt(const std::string& prompt) {
        nlohmann::json result;
        result["original_prompt"] = prompt;
        result["operations"] = nlohmann::json::array();
        result["results"] = nlohmann::json::array();
        result["daw_commands"] = nlohmann::json::array();

        try {
            // Stage 1: Identify operations
            std::cout << "Stage 1: Identifying operations..." << std::endl;
            auto identification_result = operation_identifier_->identifyOperations(prompt);

            if (!identification_result.success) {
                result["error"] = identification_result.error_message;
                return result;
            }

            std::cout << "Identified " << identification_result.operations.size() << " operations:" << std::endl;
            for (const auto& op : identification_result.operations) {
                std::cout << "  - " << op.type << ": " << op.description << std::endl;
            }

            // Stage 2: Execute operations with specialized agents
            std::cout << "\nStage 2: Executing operations..." << std::endl;

            for (const auto& operation : identification_result.operations) {
                std::cout << "Processing " << operation.type << " operation: " << operation.description << std::endl;

                // Find appropriate agent
                auto agent_it = agents_.find(operation.type);
                if (agent_it == agents_.end()) {
                    std::cout << "Warning: No agent found for operation type '" << operation.type << "'" << std::endl;
                    continue;
                }

                // Execute operation
                try {
                    auto agent_response = agent_it->second->execute(operation.description, operation.parameters);

                    // Add to results
                    result["operations"].push_back({
                        {"type", operation.type},
                        {"description", operation.description},
                        {"parameters", operation.parameters}
                    });

                    result["results"].push_back(agent_response.result);
                    result["daw_commands"].push_back(agent_response.daw_command);

                    std::cout << "  ✓ " << agent_response.daw_command << std::endl;

                } catch (const std::exception& e) {
                    std::cout << "  ✗ Error executing " << operation.type << " operation: " << e.what() << std::endl;
                }
            }

        } catch (const std::exception& e) {
            result["error"] = "Pipeline error: " + std::string(e.what());
        }

        return result;
    }

    /**
     * @brief Get information about all available agents
     * @return Agent information
     */
    nlohmann::json getAgentInfo() const {
        nlohmann::json agent_info;
        for (const auto& pair : agents_) {
            agent_info[pair.first] = {
                {"capabilities", pair.second->getCapabilities()}
            };
        }
        return agent_info;
    }

private:
    std::string api_key_;
    std::unique_ptr<magda_cpp::OperationIdentifier> operation_identifier_;
    std::map<std::string, std::shared_ptr<BaseAgent>> agents_;
};

} // namespace magda
