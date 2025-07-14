#include "magda_cpp/agents/base_agent.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace magda {

BaseAgent::BaseAgent(const std::string& name, const std::string& api_key)
    : name_(name) {
    if (!api_key.empty()) {
        client_ = std::make_unique<OpenAIClient>(api_key);
    } else {
        // Try to get API key from environment
        const char* env_key = std::getenv("OPENAI_API_KEY");
        if (env_key) {
            client_ = std::make_unique<OpenAIClient>(env_key);
        }
    }
}

nlohmann::json BaseAgent::parseOperationWithLLM(const std::string& operation,
                                                const std::string& instructions,
                                                const JsonSchemaBuilder& schema) {
    if (!client_) {
        throw std::runtime_error("OpenAI client not initialized. Please provide API key.");
    }

    try {
        // Create LLM request with structured output
        LLMRequestConfig config;
        config.client = "openai";
        config.model = ModelConfig::CURRENT_SPECIALIZED_AGENTS;  // Use current specialized agents model
        config.temperature = 0.1f;
        config.schemaObject = schema.build();
        config.functionName = "parse_operation";

        // Create the request
        LLMRequest request(config, instructions);

        // Add the operation as context
        LLMContext context = {{{"role", "user"}, {"content", operation}}};
        request.context = context;

        // Send the request
        auto response = client_->sendRequest(request);

        if (response.success) {
            std::cout << "🔍 DEBUG: Base agent response result: " << response.result.dump(2) << std::endl;
            // The response should already be structured JSON due to schema validation
            if (response.result.contains("text")) {
                std::cout << "🔍 DEBUG: Base agent text field type: " << (response.result["text"].is_string() ? "string" : "object") << std::endl;
                try {
                    // Extract the text field and parse it as JSON
                    if (response.result["text"].is_string()) {
                        std::cout << "🔍 DEBUG: Base agent parsing string: " << response.result["text"].get<std::string>() << std::endl;
                        return nlohmann::json::parse(response.result["text"].get<std::string>());
                    } else {
                        // If text is already a JSON object, return it directly
                        std::cout << "🔍 DEBUG: Base agent returning object directly" << std::endl;
                        return response.result["text"];
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    // If parsing fails, return a basic structure
                    nlohmann::json fallback;
                    fallback["error"] = "Failed to parse LLM response as JSON";
                    if (response.result["text"].is_string()) {
                        fallback["raw_response"] = response.result["text"].get<std::string>();
                    } else {
                        fallback["raw_response"] = response.result["text"].dump();
                    }
                    return fallback;
                }
            } else {
                // Return the result directly if it's already structured
                return response.result;
            }
        } else {
            nlohmann::json error_result;
            error_result["error"] = response.errorMessage;
            return error_result;
        }
    } catch (const std::exception& e) {
        nlohmann::json error_result;
        error_result["error"] = std::string("LLM request failed: ") + e.what();
        return error_result;
    }
}

std::string BaseAgent::getTrackIdFromContext(const nlohmann::json& context) const {
    // Try to get track_id directly
    if (context.contains("track_id") && context["track_id"].is_string()) {
        return context["track_id"].get<std::string>();
    }

    // Try to get track from context
    if (context.contains("track") && context["track"].is_object()) {
        auto track = context["track"];
        if (track.contains("id") && track["id"].is_string()) {
            return track["id"].get<std::string>();
        }
    }

    return "";
}

std::string BaseAgent::getTrackNameFromContext(const nlohmann::json& context) const {
    // Try to get track_name directly
    if (context.contains("track_name") && context["track_name"].is_string()) {
        return context["track_name"].get<std::string>();
    }

    // Try to get track from context
    if (context.contains("track") && context["track"].is_object()) {
        auto track = context["track"];
        if (track.contains("name") && track["name"].is_string()) {
            return track["name"].get<std::string>();
        }
    }

    return "unknown";
}

std::string BaseAgent::generateUniqueId() const {
    // Generate a simple UUID-like string
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 65535);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << (millis & 0xFFFFFFFF);
    oss << "-";
    oss << std::hex << std::setfill('0') << std::setw(4) << dis(gen);
    oss << "-";
    oss << std::hex << std::setfill('0') << std::setw(4) << dis(gen);
    oss << "-";
    oss << std::hex << std::setfill('0') << std::setw(4) << dis(gen);
    oss << "-";
    oss << std::hex << std::setfill('0') << std::setw(12) << (dis(gen) << 16 | dis(gen));

    return oss.str();
}

} // namespace magda
