#include "magda_cpp/pipeline.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#include <iostream>

namespace magda {

MAGDAPipeline::MAGDAPipeline(const std::string& api_key) : api_key_(api_key) {
    initializeAgents();
}

void MAGDAPipeline::initializeAgents() {
    // Initialize operation identifier
    operation_identifier_ = std::make_unique<OperationIdentifier>(api_key_);

    // Initialize specialized agents
    agents_["track"] = std::make_unique<TrackAgent>(api_key_);
    // agents_["clip"] = std::make_unique<ClipAgent>(api_key_);
    agents_["volume"] = std::make_unique<VolumeAgent>(api_key_);
    // agents_["effect"] = std::make_unique<EffectAgent>(api_key_);
    // agents_["midi"] = std::make_unique<MidiAgent>(api_key_);
}

std::optional<PipelineResult> MAGDAPipeline::processPrompt(const std::string& prompt) {
    try {
        std::vector<Operation> operations;
        std::vector<std::string> daw_commands;

        // Stage 1: Identify operations
        std::cout << "Stage 1: Identifying operations..." << std::endl;
        auto identification_result = operation_identifier_->identifyOperations(prompt);

        if (!identification_result.success) {
            std::cout << "Error identifying operations: " << identification_result.error_message << std::endl;
            return std::nullopt;
        }

        std::cout << "Identified " << identification_result.operations.size() << " operations:" << std::endl;
        for (const auto& op : identification_result.operations) {
            std::cout << "  - " << op.type << ": " << op.description << std::endl;
        }

        // Stage 2: Execute operations with specialized agents
        std::cout << "\nStage 2: Executing operations..." << std::endl;

        for (const auto& daw_operation : identification_result.operations) {
            std::cout << "Processing " << daw_operation.type << " operation: " << daw_operation.description << std::endl;

            // Convert DAWOperation to Operation
            OperationType op_type = OperationType::UNKNOWN;
            if (daw_operation.type == "track") op_type = OperationType::CREATE_TRACK;
            else if (daw_operation.type == "clip") op_type = OperationType::CREATE_CLIP;
            else if (daw_operation.type == "volume") op_type = OperationType::SET_VOLUME;
            else if (daw_operation.type == "effect") op_type = OperationType::ADD_EFFECT;
            else if (daw_operation.type == "midi") op_type = OperationType::CREATE_MIDI;

            Operation operation(op_type, daw_operation.parameters, daw_operation.type);

            // Find appropriate agent
            auto agent_it = agents_.find(daw_operation.type);
            if (agent_it == agents_.end()) {
                std::cout << "Warning: No agent found for operation type '" << daw_operation.type << "'" << std::endl;
                continue;
            }

            // Execute operation
            try {
                auto agent_response = agent_it->second->execute(daw_operation.description, context_);

                // Add to results
                operations.push_back(operation);
                daw_commands.push_back(agent_response.daw_command);

                // Update context with agent response
                if (!agent_response.context.is_null()) {
                    context_.merge_patch(agent_response.context);
                }

                std::cout << "  ✓ " << agent_response.daw_command << std::endl;

            } catch (const std::exception& e) {
                std::cout << "  ✗ Error executing " << daw_operation.type << " operation: " << e.what() << std::endl;
            }
        }

        return PipelineResult(operations, daw_commands, context_);

    } catch (const std::exception& e) {
        std::cout << "Pipeline error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

BaseAgent* MAGDAPipeline::findAgentForOperation(const std::string& operation) {
    for (auto& [name, agent] : agents_) {
        if (agent->canHandle(operation)) {
            return agent.get();
        }
    }
    return nullptr;
}

} // namespace magda
