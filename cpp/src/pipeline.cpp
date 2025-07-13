#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"
#include <memory>
#include <vector>
#include <algorithm>

namespace magda {

/**
 * @brief Pipeline class that coordinates multiple agents
 */
class Pipeline {
public:
    Pipeline() = default;

    /**
     * @brief Add an agent to the pipeline
     * @param agent Shared pointer to the agent
     */
    void addAgent(std::shared_ptr<BaseAgent> agent) {
        agents_.push_back(agent);
    }

    /**
     * @brief Process an operation by finding the appropriate agent
     * @param operation The operation string
     * @param context Optional context
     * @return AgentResponse from the appropriate agent
     */
    AgentResponse processOperation(const std::string& operation,
                                   const nlohmann::json& context = nlohmann::json::object()) {
        // Find the first agent that can handle this operation
        for (auto& agent : agents_) {
            if (agent->canHandle(operation)) {
                return agent->execute(operation, context);
            }
        }

        // If no agent can handle it, return an error response
        nlohmann::json error_result;
        error_result["error"] = "No agent found to handle operation: " + operation;
        
        return AgentResponse(error_result, "", context);
    }

    /**
     * @brief Get all agents in the pipeline
     * @return Vector of agent pointers
     */
    const std::vector<std::shared_ptr<BaseAgent>>& getAgents() const {
        return agents_;
    }

    /**
     * @brief Create a default pipeline with common agents
     * @param api_key OpenAI API key
     * @return Pipeline with track and volume agents
     */
    static Pipeline createDefaultPipeline(const std::string& api_key = "") {
        Pipeline pipeline;
        pipeline.addAgent(std::make_shared<TrackAgent>(api_key));
        pipeline.addAgent(std::make_shared<VolumeAgent>(api_key));
        return pipeline;
    }

private:
    std::vector<std::shared_ptr<BaseAgent>> agents_;
};

} // namespace magda 