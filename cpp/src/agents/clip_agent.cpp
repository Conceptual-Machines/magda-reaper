#include "magda_cpp/agents/clip_agent.h"
#include <llmcpp/core/LLMRequest.h>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace magda_cpp {

ClipAgent::ClipAgent(const std::string& api_key) {
    // Initialize OpenAI client
    llmcpp::OpenAI::Config config;
    if (!api_key.empty()) {
        config.api_key = api_key;
    }
    client_ = std::make_unique<llmcpp::OpenAI::OpenAIClient>(config);
}

bool ClipAgent::canHandle(const std::string& operation) const {
    std::string op_lower = operation;
    std::transform(op_lower.begin(), op_lower.end(), op_lower.begin(), ::tolower);

    return op_lower.find("clip") != std::string::npos ||
           op_lower.find("region") != std::string::npos ||
           op_lower.find("recording") != std::string::npos ||
           op_lower.find("audio clip") != std::string::npos ||
           op_lower.find("midi clip") != std::string::npos;
}

AgentResponse ClipAgent::execute(const std::string& operation, const nlohmann::json& context) {
    try {
        // Parse clip operation using LLM
        auto clip_info = parseClipOperationWithLLM(operation);

        // Get track ID from context
        std::string track_id = getTrackIdFromContext(context);

        // Generate unique clip ID
        std::string clip_id = generateUniqueId();

        // Create clip result
        ClipResult clip_result;
        clip_result.clip_id = clip_id;
        clip_result.track_id = track_id;
        clip_result.start_bar = clip_info.value("start_bar", 1);
        clip_result.end_bar = clip_info.value("end_bar", clip_result.start_bar + 4);

        // Handle optional parameters
        if (clip_info.contains("start_time")) {
            clip_result.start_time = clip_info["start_time"];
        }
        if (clip_info.contains("duration")) {
            clip_result.duration = clip_info["duration"];
        }
        if (clip_info.contains("track_name")) {
            clip_result.track_name = clip_info["track_name"];
        }

        // Store clip for future reference
        clips_[clip_id] = clip_result;

        // Generate DAW command
        std::string daw_command = generateDawCommand(clip_result);

        // Create result JSON
        nlohmann::json result;
        result["id"] = clip_id;
        result["track_id"] = track_id;
        result["start_bar"] = clip_result.start_bar;
        result["end_bar"] = clip_result.end_bar;

        if (clip_result.start_time.has_value()) {
            result["start_time"] = clip_result.start_time.value();
        }
        if (clip_result.duration.has_value()) {
            result["duration"] = clip_result.duration.value();
        }
        if (!clip_result.track_name.empty()) {
            result["track_name"] = clip_result.track_name;
        }

        return AgentResponse(result, daw_command, context);

    } catch (const std::exception& e) {
        nlohmann::json error_result;
        error_result["error"] = "Error executing clip operation: " + std::string(e.what());
        return AgentResponse(error_result, "", context);
    }
}

std::vector<std::string> ClipAgent::getCapabilities() const {
    return {"clip", "region", "recording", "audio clip", "midi clip"};
}

std::optional<ClipResult> ClipAgent::getClipById(const std::string& clip_id) const {
    auto it = clips_.find(clip_id);
    if (it != clips_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ClipResult> ClipAgent::listClips() const {
    std::vector<ClipResult> result;
    for (const auto& pair : clips_) {
        result.push_back(pair.second);
    }
    return result;
}

nlohmann::json ClipAgent::parseClipOperationWithLLM(const std::string& operation) {
    try {
        llmcpp::LLMRequest request;
        request.model = llmcpp::OpenAI::Model::GPT_4O_MINI;
        request.system_prompt = R"(
You are a clip specialist for a DAW system.
Your job is to parse clip requests and extract the necessary parameters.

Extract the following information:
- start_bar: Starting bar number (default: 1)
- end_bar: Ending bar number (default: start_bar + 4)
- start_time: Start time in seconds (optional)
- duration: Clip duration in seconds (optional)
- track_name: Target track name (optional)

Return a JSON object with the extracted parameters.
)";
        request.user_prompt = operation;
        request.temperature = 0.1f;
        request.max_tokens = 500;

        // Add JSON schema for structured output
        request.json_schema = {
            {"type", "object"},
            {"properties", {
                {"start_bar", {{"type", "integer"}}},
                {"end_bar", {{"type", "integer"}}},
                {"start_time", {{"type", "number"}}},
                {"duration", {{"type", "number"}}},
                {"track_name", {{"type", "string"}}}
            }},
            {"required", {"start_bar"}}
        };

        auto response = client_->chat(request);

        if (response.success && response.json_content.has_value()) {
            return response.json_content.value();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing clip operation: " << e.what() << std::endl;
    }

    // Return default values if parsing fails
    return {
        {"start_bar", 1},
        {"end_bar", 5}
    };
}

std::string ClipAgent::generateDawCommand(const ClipResult& clip) const {
    std::ostringstream oss;
    oss << "clip(track:" << clip.track_id
        << ", start_bar:" << clip.start_bar
        << ", end_bar:" << clip.end_bar;

    if (clip.start_time.has_value()) {
        oss << ", start_time:" << clip.start_time.value();
    }
    if (clip.duration.has_value()) {
        oss << ", duration:" << clip.duration.value();
    }

    oss << ")";
    return oss.str();
}

std::string ClipAgent::getTrackIdFromContext(const nlohmann::json& context) const {
    // Try to get track ID from context parameters
    if (context.contains("track_id")) {
        return context["track_id"];
    }
    if (context.contains("track_daw_id")) {
        return context["track_daw_id"];
    }

    // Fallback to old context method
    if (context.contains("track") && context["track"].contains("id")) {
        return context["track"]["id"];
    }

    return "unknown";
}

} // namespace magda_cpp
