#include "magda_cpp/agents/operation_identifier.h"
#include "prompt_loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace magda {

OperationIdentifier::OperationIdentifier(const std::string& api_key)
    : BaseAgent("operation_identifier", api_key) {
}

OperationIdentificationResult OperationIdentifier::identifyOperations(const std::string& prompt) {
    OperationIdentificationResult result;
    result.original_prompt = prompt;
    result.success = false;

    try {
        LLMRequestConfig config;
        config.client = "openai";
        config.model = ModelConfig::CURRENT_DECISION_AGENT;

        // Only set temperature and maxTokens if the model supports them
        if (config.model != std::string("o3-mini")) {
            config.temperature = 0.1f;
            config.maxTokens = 1000;
        }

        std::string system_prompt = buildSystemPrompt();
        std::string full_prompt = system_prompt + "\n" + prompt;
        LLMContext context = {};

        // Check if we should use structured output (schema-based) or free-form output
        bool use_structured_output = shouldUseStructuredOutput();

        std::cout << "🔍 DEBUG: Model: " << config.model << std::endl;
        std::cout << "🔍 DEBUG: Using structured output: " << (use_structured_output ? "YES" : "NO") << std::endl;
        std::cout << "🔍 DEBUG: Temperature: " << (config.temperature.has_value() ? std::to_string(config.temperature.value()) : "not set") << std::endl;
        std::cout << "🔍 DEBUG: Max tokens: " << (config.maxTokens.has_value() ? std::to_string(config.maxTokens.value()) : "not set") << std::endl;

        LLMRequest request = use_structured_output
            ? LLMRequest(config, full_prompt, context, getOperationSchema())
            : LLMRequest(config, full_prompt, context);

        auto response = client_->sendRequest(request);

        std::cout << "🔍 DEBUG: Response success: " << (response.success ? "YES" : "NO") << std::endl;
        std::cout << "🔍 DEBUG: Response type: " << typeid(response.result).name() << std::endl;
        std::cout << "🔍 DEBUG: Response result: " << response.result.dump() << std::endl;

        if (response.success) {
            if (use_structured_output) {
                result.operations = parseStructuredOperations(response);
            } else {
                result.operations = parseFreeFormOperations(response);
            }
            result.success = true;
        } else {
            result.error_message = "LLM request failed: " + response.errorMessage;
        }

    } catch (const std::exception& e) {
        result.error_message = "Exception during operation identification: " + std::string(e.what());
    }

    return result;
}

bool OperationIdentifier::shouldUseStructuredOutput() {
    // Use structured output for models that support it well (like gpt-4.1)
    // Use free-form output for reasoning models (like o3-mini)
    std::string model = ModelConfig::CURRENT_DECISION_AGENT;

    if (model.find("o3") != std::string::npos ||
        model.find("o1") != std::string::npos ||
        model.find("o4") != std::string::npos) {
        return false;  // Reasoning models - use free-form output
    }

    return true;  // Other models - use structured output
}

std::string OperationIdentifier::getRecommendedModel() {
    return ModelConfig::CURRENT_DECISION_AGENT; // Use current decision agent model
}

nlohmann::json OperationIdentifier::getOperationSchema() {
    return {
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", {
            {"operations", {
                {"type", "array"},
                {"items", {
                    {"type", "object"},
                    {"additionalProperties", false},
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
        static SharedResources resources;
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

std::vector<DAWOperation> OperationIdentifier::parseStructuredOperations(const LLMResponse& response) {
    std::vector<DAWOperation> operations;

    try {
        // Parse structured output (like gpt-4.1 with schema)
        // The response should contain structured data directly
        std::cout << "🔍 DEBUG: Parsing structured operations" << std::endl;

        // For structured output, the response should already be parsed
        // This would be similar to Python's response.output_parsed
        // Implementation depends on how the llmcpp library handles structured responses

        // For now, fall back to free-form parsing as a safety measure
        return parseFreeFormOperations(response);

    } catch (const std::exception& e) {
        std::cerr << "Error parsing structured operations: " << e.what() << std::endl;
    }

    return operations;
}

std::vector<DAWOperation> OperationIdentifier::parseFreeFormOperations(const LLMResponse& response) {
    std::vector<DAWOperation> operations;

    try {
        // Parse free-form text output (like o3-mini)
        // The response is a JSON object with a "text" field containing the actual JSON string
        std::cout << "🔍 DEBUG: Parsing free-form operations" << std::endl;

        // Extract the text field from the response
        if (response.result.contains("text") && response.result["text"].is_string()) {
            std::string json_text = response.result["text"];
            std::cout << "🔍 DEBUG: Extracted text: " << json_text << std::endl;

            // Parse the JSON string
            auto result = nlohmann::json::parse(json_text);
            std::cout << "🔍 DEBUG: Parsed JSON: " << result.dump(2) << std::endl;

            if (result.contains("operations") && result["operations"].is_array()) {
                for (const auto& op : result["operations"]) {
                    std::cout << "🔍 DEBUG: Processing operation: " << op.dump(2) << std::endl;

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
            } else {
                std::cout << "🔍 DEBUG: No 'operations' array found in JSON" << std::endl;
            }
        } else {
            std::cout << "🔍 DEBUG: No 'text' field found in response" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing free-form operations: " << e.what() << std::endl;
    }

    return operations;
}

// BaseAgent interface implementation
bool OperationIdentifier::canHandle(const std::string& operation) const {
    // OperationIdentifier can handle any operation type
    return true;
}

AgentResponse OperationIdentifier::execute(const std::string& operation, const nlohmann::json& context) {
    // Execute operation identification
    auto result = identifyOperations(operation);

    if (result.success) {
        // Convert DAWOperations to Operations
        std::vector<Operation> operations;
        for (const auto& daw_op : result.operations) {
            OperationType op_type = OperationType::UNKNOWN;
            if (daw_op.type == "track") op_type = OperationType::CREATE_TRACK;
            else if (daw_op.type == "clip") op_type = OperationType::CREATE_CLIP;
            else if (daw_op.type == "volume") op_type = OperationType::SET_VOLUME;
            else if (daw_op.type == "effect") op_type = OperationType::ADD_EFFECT;
            else if (daw_op.type == "midi") op_type = OperationType::CREATE_MIDI;

            operations.emplace_back(op_type, daw_op.parameters, daw_op.type);
        }

        // Create response
        nlohmann::json result_json;
        result_json["operations"] = nlohmann::json::array();
        for (const auto& op : operations) {
            result_json["operations"].push_back({
                {"type", op.agent_name},
                {"description", op.toString()},
                {"parameters", op.parameters}
            });
        }

        return AgentResponse(result_json, "operation_identification", context);
    } else {
        throw std::runtime_error("Operation identification failed: " + result.error_message);
    }
}

std::vector<std::string> OperationIdentifier::getCapabilities() const {
    return {"operation_identification", "track", "clip", "volume", "effect", "midi"};
}

std::string OperationIdentifier::generateDAWCommand(const nlohmann::json& result) const {
    // OperationIdentifier doesn't generate DAW commands directly
    return "operation_identification";
}

} // namespace magda
