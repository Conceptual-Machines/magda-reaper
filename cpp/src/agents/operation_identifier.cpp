#include "magda_cpp/agents/operation_identifier.h"
#include "prompt_loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace magda_cpp {

OperationIdentifier::OperationIdentifier(const std::string& api_key) {
    // Initialize OpenAI client
    llmcpp::OpenAI::Config config;
    if (!api_key.empty()) {
        config.api_key = api_key;
    }
    client_ = std::make_unique<llmcpp::OpenAI::OpenAIClient>(config);
}

OperationIdentificationResult OperationIdentifier::identifyOperations(const std::string& prompt) {
    OperationIdentificationResult result;
    result.original_prompt = prompt;
    result.success = false;

    try {
        // Build the request
        llmcpp::LLMRequest request;
        request.model = getRecommendedModel();
        request.system_prompt = buildSystemPrompt();
        request.user_prompt = prompt;
        request.temperature = 0.1f;
        request.max_tokens = 1000;

        // Add JSON schema for structured output
        request.json_schema = getOperationSchema();

        // Make the request
        auto response = client_->chat(request);

        if (response.success) {
            result.operations = parseOperations(response);
            result.success = true;
        } else {
            result.error_message = "LLM request failed: " + response.error_message;
        }

    } catch (const std::exception& e) {
        result.error_message = "Exception during operation identification: " + std::string(e.what());
    }

    return result;
}

llmcpp::OpenAI::Model OperationIdentifier::getRecommendedModel() {
    return llmcpp::OpenAI::Model::GPT_4O_MINI; // Cost-effective for operation identification
}

nlohmann::json OperationIdentifier::getOperationSchema() {
    return {
        {"type", "object"},
        {"properties", {
            {"operations", {
                {"type", "array"},
                {"items", {
                    {"type", "object"},
                    {"properties", {
                        {"type", {
                            {"type", "string"},
                            {"enum", {"track", "clip", "volume", "effect", "midi"}}
                        }},
                        {"description", {{"type", "string"}}},
                        {"parameters", {{"type", "object"}}}
                    }},
                    {"required", {"type", "description", "parameters"}}
                }}
            }}
        }},
        {"required", {"operations"}}
    };
}

std::string OperationIdentifier::buildSystemPrompt() {
    try {
        // Try to use shared prompt loader
        auto& resources = shared::getSharedResources();
        return resources.getOperationIdentifierPrompt();
    } catch (...) {
        // Fallback to hardcoded prompt
        return R"(
You are an operation identifier for a DAW (Digital Audio Workstation) system.
Your job is to analyze natural language prompts and break them down into discrete operations.

For each operation, return an object with:
- type: the operation type (track, clip, volume, effect, midi)
- description: a short human-readable description of the operation
- parameters: a dictionary of parameters for the operation

Return your analysis as a JSON object with an 'operations' array, where each operation has 'type', 'description', and 'parameters'.

Example output:
{"operations": [
  {"type": "track", "description": "Create a track with Serum VST named 'bass'", "parameters": {"name": "bass", "vst": "serum"}},
  {"type": "clip", "description": "Add a clip starting from bar 17", "parameters": {"start_bar": 17}}
]}
)";
    }
}

std::vector<DAWOperation> OperationIdentifier::parseOperations(const llmcpp::LLMResponse& response) {
    std::vector<DAWOperation> operations;

    try {
        if (response.json_content.has_value()) {
            auto json = response.json_content.value();

            if (json.contains("operations") && json["operations"].is_array()) {
                for (const auto& op : json["operations"]) {
                    DAWOperation operation;

                    if (op.contains("type") && op["type"].is_string()) {
                        operation.type = op["type"];
                    }

                    if (op.contains("description") && op["description"].is_string()) {
                        operation.description = op["description"];
                    }

                    if (op.contains("parameters") && op["parameters"].is_object()) {
                        operation.parameters = op["parameters"];
                    }

                    operations.push_back(operation);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing operations: " << e.what() << std::endl;
    }

    return operations;
}

} // namespace magda_cpp
