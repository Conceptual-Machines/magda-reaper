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

        // Set temperature for models that support it
        config.temperature = 0.1f;

        std::string system_prompt = buildSystemPrompt();
        std::string full_prompt = system_prompt + "\n" + prompt;
        LLMContext context = {};

        // Check if we should use structured output (schema-based) or free-form output
        bool use_structured_output = shouldUseStructuredOutput();

        std::cout << "🔍 DEBUG: Model: " << config.model << std::endl;
        std::cout << "🔍 DEBUG: Using structured output: " << (use_structured_output ? "YES" : "NO") << std::endl;
        std::cout << "🔍 DEBUG: Temperature: " << (config.temperature.has_value() ? std::to_string(config.temperature.value()) : "not set") << std::endl;

        LLMRequest request = [&]() -> LLMRequest {
            if (use_structured_output) {
                // Use JsonSchemaBuilder for structured output
                auto schema = JsonSchemaBuilder()
                    .type("object")
                    .title("Operation Identification")
                    .description("Identify operations from natural language prompt")
                    .property("operations", JsonSchemaBuilder()
                        .type("array")
                        .description("Array of identified operations")
                        .items(JsonSchemaBuilder()
                            .type("object")
                            .description("Individual operation")
                            .property("type", JsonSchemaBuilder()
                                .type("string")
                                .description("Operation type")
                                .enumValues({"track", "clip", "volume", "effect", "midi"}))
                            .property("description", JsonSchemaBuilder()
                                .type("string")
                                .description("Human-readable description"))
                                                    .property("parameters", JsonSchemaBuilder()
                            .type("object")
                            .description("Operation parameters")
                            .additionalProperties(false))
                        .required({"type", "description"})
                        .additionalProperties(false)))
                    .required({"operations"})
                    .additionalProperties(false);

                // Convert schema to JSON and set it in the config
                config.schemaObject = schema.build();
                config.functionName = "identify_operations";
                return LLMRequest(config, full_prompt, context);
            } else {
                return LLMRequest(config, full_prompt, context);
            }
        }();

        auto response = client_->sendRequest(request);

        // Print the raw LLM response before any parsing
        std::cout << "🔍 DEBUG: Response result (raw, pre-parse): " << response.result.dump(2) << std::endl;
        std::cout << "🔍 DEBUG: Response success: " << (response.success ? "YES" : "NO") << std::endl;
        std::cout << "🔍 DEBUG: Response type: " << typeid(response.result).name() << std::endl;

        // Print detailed response structure
        std::cout << "🔍 DEBUG: Response.result type: " << (response.result.is_object() ? "object" : response.result.is_string() ? "string" : "other") << std::endl;
        std::cout << "🔍 DEBUG: Response.result keys: ";
        if (response.result.is_object()) {
            for (auto it = response.result.begin(); it != response.result.end(); ++it) {
                std::cout << "'" << it.key() << "'(" << (it.value().is_string() ? "string" : it.value().is_object() ? "object" : "other") << ") ";
            }
        }
        std::cout << std::endl;

        // Print each field individually
        if (response.result.is_object()) {
            for (auto it = response.result.begin(); it != response.result.end(); ++it) {
                std::cout << "🔍 DEBUG: Field '" << it.key() << "' = " << it.value().dump(2) << std::endl;
            }
        }

        try {
            if (response.success) {
                if (use_structured_output) {
                    std::cout << "🔍 DEBUG: About to call parseStructuredOperations" << std::endl;
                    result.operations = parseStructuredOperations(response);
                } else {
                    result.operations = parseFreeFormOperations(response);
                }
                result.success = true;
            } else {
                result.error_message = "LLM request failed: " + response.errorMessage;
            }
        } catch (const std::exception& e) {
            std::cerr << "🔍 EXCEPTION during result parsing: " << e.what() << std::endl;
            std::cerr << "🔍 DEBUG: Response.result at exception: " << response.result.dump(2) << std::endl;
            result.error_message = std::string("Exception during result parsing: ") + e.what();
        }

    } catch (const std::exception& e) {
        std::cerr << "🔍 EXCEPTION in identifyOperations: " << e.what() << std::endl;
        result.error_message = "Exception during operation identification: " + std::string(e.what());
    }

    return result;
}

bool OperationIdentifier::shouldUseStructuredOutput() {
    // Use structured output for models that support it well (like gpt-4.1-mini)
    // Use free-form output for reasoning models (like o3-mini)
    std::string model = ModelConfig::CURRENT_DECISION_AGENT;

    if (model.find("o3") != std::string::npos ||
        model.find("o1") != std::string::npos ||
        model.find("o4") != std::string::npos) {
        return false;  // Reasoning models - use free-form output
    }

    return true;  // Other models (gpt-4.1-mini, gpt-4o-mini, etc.) - use structured output
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
                    {"required", {"type", "description"}}
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
        // Parse structured output (like gpt-4.1-mini with JsonSchemaBuilder)
        std::cout << "🔍 DEBUG: Parsing structured operations" << std::endl;
        std::cout << "🔍 DEBUG: Response structure: " << response.result.dump(2) << std::endl;

        // For JsonSchemaBuilder structured output, the response should contain the parsed data directly
        // Check if the response has the expected structure
        if (response.result.contains("operations") && response.result["operations"].is_array()) {
            std::cout << "🔍 DEBUG: Found operations array in structured response" << std::endl;

            for (const auto& op : response.result["operations"]) {
                std::cout << "🔍 DEBUG: Processing structured operation: " << op.dump(2) << std::endl;

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
        } else if (response.result.contains("text") && response.result["text"].is_string()) {
            // Fallback: if the response has a "text" field, try to parse it as JSON
            std::cout << "🔍 DEBUG: Found 'text' field in structured response, parsing as JSON" << std::endl;
            try {
                auto parsed = nlohmann::json::parse(response.result["text"].get<std::string>());
                if (parsed.contains("operations") && parsed["operations"].is_array()) {
                    for (const auto& op : parsed["operations"]) {
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
            } catch (const nlohmann::json::parse_error& e) {
                std::cout << "🔍 DEBUG: Failed to parse 'text' field as JSON: " << e.what() << std::endl;
            }
        } else {
            std::cout << "🔍 DEBUG: No 'operations' array or 'text' field found in structured response, falling back to free-form parsing" << std::endl;
            // Fall back to free-form parsing if structured parsing fails
            return parseFreeFormOperations(response);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing structured operations: " << e.what() << std::endl;
        std::cout << "🔍 DEBUG: Falling back to free-form parsing due to error" << std::endl;
        // Fall back to free-form parsing if structured parsing fails
        return parseFreeFormOperations(response);
    }

    return operations;
}

std::vector<DAWOperation> OperationIdentifier::parseFreeFormOperations(const LLMResponse& response) {
    std::vector<DAWOperation> operations;

    try {
        // Parse free-form text output (like o3-mini)
        // The response is a JSON object with a "text" field containing the actual JSON string or object
        std::cout << "🔍 DEBUG: Parsing free-form operations" << std::endl;

        // Extract the text field from the response
        if (response.result.contains("text")) {
            const auto& text_field = response.result["text"];
            std::cout << "🔍 DEBUG: 'text' field type: " << (text_field.is_string() ? "string" : text_field.is_object() ? "object" : "other") << std::endl;
            std::cout << "🔍 DEBUG: 'text' field content: " << text_field.dump(2) << std::endl;

            if (text_field.is_string()) {
                std::string json_text = text_field;
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
            } else if (text_field.is_object()) {
                // Already parsed JSON object
                const auto& result = text_field;
                std::cout << "🔍 DEBUG: Parsed JSON (object): " << result.dump(2) << std::endl;
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
                    std::cout << "🔍 DEBUG: No 'operations' array found in JSON object" << std::endl;
                }
            } else {
                std::cout << "🔍 DEBUG: 'text' field is neither string nor object" << std::endl;
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
