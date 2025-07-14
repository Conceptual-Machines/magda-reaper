#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/agents/orchestrator_agent.h"
#include "magda_cpp/agents/track_agent.h"
#include "magda_cpp/agents/volume_agent.h"
#include "magda_cpp/agents/effect_agent.h"
#include "magda_cpp/agents/clip_agent.h"
#include "magda_cpp/agents/midi_agent.h"
#include "magda_cpp/models.h"

namespace magda {

/**
 * @brief Result structure for pipeline processing
 */
struct PipelineResult {
    std::vector<Operation> operations;
    std::vector<std::string> daw_commands;
    nlohmann::json context;

    PipelineResult() = default;
    PipelineResult(const std::vector<Operation>& ops,
                   const std::vector<std::string>& commands,
                   const nlohmann::json& ctx = nlohmann::json::object())
        : operations(ops), daw_commands(commands), context(ctx) {}
};

/**
 * @brief Main MAGDA pipeline class
 *
 * This class orchestrates the two-stage pipeline:
 * 1. Operation orchestration using the OrchestratorAgent
 * 2. Operation execution using specialized agents
 */
class MAGDAPipeline {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (optional, will try environment variable if not provided)
     */
    MAGDAPipeline(const std::string& api_key = "");

    /**
     * @brief Process a natural language prompt through the pipeline
     * @param prompt The user's natural language prompt
     * @return Optional PipelineResult containing operations and DAW commands
     */
    std::optional<PipelineResult> processPrompt(const std::string& prompt);

    /**
     * @brief Get the current context
     * @return Current context as JSON
     */
    nlohmann::json getContext() const { return context_; }

    /**
     * @brief Set the context
     * @param context New context
     */
    void setContext(const nlohmann::json& context) { context_ = context; }

private:
    std::string api_key_;
    nlohmann::json context_;

    // Agents
    std::unique_ptr<OrchestratorAgent> orchestrator_agent_;
    std::map<std::string, std::unique_ptr<BaseAgent>> agents_;

    /**
     * @brief Initialize all agents
     */
    void initializeAgents();

    /**
     * @brief Find the appropriate agent for an operation
     * @param operation The operation to find an agent for
     * @return Pointer to the appropriate agent, or nullptr if not found
     */
    BaseAgent* findAgentForOperation(const std::string& operation);
};

} // namespace magda
