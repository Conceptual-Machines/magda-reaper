#include "magda_cpp/agents/effect_agent.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace magda {

EffectAgent::EffectAgent(const std::string& api_key) {
    // Initialize OpenAI client
    OpenAI::Config config;
    if (!api_key.empty()) {
        config.api_key = api_key;
    }
    client_ = std::make_unique<OpenAI::OpenAIClient>(config);
}

bool EffectAgent::canHandle(const std::string& operation) const {
    std::string op_lower = operation;
    std::transform(op_lower.begin(), op_lower.end(), op_lower.begin(), ::tolower);

    return op_lower.find("effect") != std::string::npos ||
           op_lower.find("reverb") != std::string::npos ||
           op_lower.find("delay") != std::string::npos ||
           op_lower.find("compressor") != std::string::npos ||
           op_lower.find("eq") != std::string::npos ||
           op_lower.find("filter") != std::string::npos ||
           op_lower.find("distortion") != std::string::npos;
}

AgentResponse EffectAgent::execute(const std::string& operation, const nlohmann::json& context) {
    try {
        // Parse effect operation using LLM
        auto effect_info = parseEffectOperationWithLLM(operation);

        // Get track ID from context
        std::string track_id = getTrackIdFromContext(context);

        // Generate unique effect ID
        std::string effect_id = generateUniqueId();

        // Create effect result
        EffectResult effect_result;
        effect_result.track_id = track_id;
        effect_result.effect_type = effect_info.value("effect_type", "reverb");
        effect_result.position = effect_info.value("position", "insert");

        // Create effect parameters if specified
        if (effect_info.contains("parameters") && effect_info["parameters"].is_object()) {
            EffectParameters params;
            auto params_json = effect_info["parameters"];

            if (params_json.contains("wet_mix")) params.wet_mix = params_json["wet_mix"];
            if (params_json.contains("dry_mix")) params.dry_mix = params_json["dry_mix"];
            if (params_json.contains("threshold")) params.threshold = params_json["threshold"];
            if (params_json.contains("ratio")) params.ratio = params_json["ratio"];
            if (params_json.contains("attack")) params.attack = params_json["attack"];
            if (params_json.contains("release")) params.release = params_json["release"];
            if (params_json.contains("decay")) params.decay = params_json["decay"];
            if (params_json.contains("feedback")) params.feedback = params_json["feedback"];
            if (params_json.contains("delay_time")) params.delay_time = params_json["delay_time"];
            if (params_json.contains("frequency")) params.frequency = params_json["frequency"];
            if (params_json.contains("q_factor")) params.q_factor = params_json["q_factor"];
            if (params_json.contains("gain")) params.gain = params_json["gain"];

            effect_result.parameters = params;
        }

        // Store effect for future reference
        effects_[effect_id] = effect_result;

        // Generate DAW command
        std::string daw_command = generateDawCommand(effect_result);

        // Create result JSON
        nlohmann::json result;
        result["id"] = effect_id;
        result["track_id"] = track_id;
        result["effect_type"] = effect_result.effect_type;
        result["position"] = effect_result.position;
        if (effect_result.parameters.has_value()) {
            result["parameters"] = effect_result.parameters.value().toJson();
        }

        return AgentResponse(result, daw_command, context);

    } catch (const std::exception& e) {
        nlohmann::json error_result;
        error_result["error"] = "Error executing effect operation: " + std::string(e.what());
        return AgentResponse(error_result, "", context);
    }
}

std::vector<std::string> EffectAgent::getCapabilities() const {
    return {"effect", "reverb", "delay", "compressor", "eq", "filter", "distortion"};
}

std::optional<EffectResult> EffectAgent::getEffectById(const std::string& effect_id) const {
    auto it = effects_.find(effect_id);
    if (it != effects_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<EffectResult> EffectAgent::listEffects() const {
    std::vector<EffectResult> result;
    for (const auto& pair : effects_) {
        result.push_back(pair.second);
    }
    return result;
}

nlohmann::json EffectAgent::parseEffectOperationWithLLM(const std::string& operation) {
    try {
        LLMRequest request;
        request.model = OpenAI::Model::GPT_4o_Mini;
        request.system_prompt = R"(
You are an effect specialist for a DAW system.
Your job is to parse effect requests and extract the necessary parameters.

Extract the following information:
- effect_type: The type of effect (reverb, delay, compressor, eq, filter, distortion, etc.)
- parameters: A dictionary of effect parameters (e.g., {"wet": 0.5, "decay": 2.0})
- position: Where to insert the effect (insert, send, master, default: insert)

Return a JSON object with the extracted parameters.
)";
        request.user_prompt = operation;
        request.temperature = 0.1f;
        request.max_tokens = 500;

        // Add JSON schema for structured output
        request.json_schema = {
            {"type", "object"},
            {"properties", {
                {"effect_type", {{"type", "string"}}},
                {"parameters", {{"type", "object"}}},
                {"position", {{"type", "string"}}}
            }},
            {"required", {"effect_type"}}
        };

        auto response = client_->chat(request);

        if (response.success && response.json_content.has_value()) {
            return response.json_content.value();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing effect operation: " << e.what() << std::endl;
    }

    // Return default values if parsing fails
    return {
        {"effect_type", "reverb"},
        {"position", "insert"},
        {"parameters", nlohmann::json::object()}
    };
}

std::string EffectAgent::generateDawCommand(const EffectResult& effect) const {
    std::ostringstream oss;
    oss << "effect(track:" << effect.track_id
        << ", type:" << effect.effect_type
        << ", position:" << effect.position;

    if (effect.parameters.has_value()) {
        const auto& params = effect.parameters.value();
        oss << ", params:{";

        std::vector<std::string> param_parts;
        param_parts.push_back("wet_mix:" + std::to_string(params.wet_mix));
        param_parts.push_back("dry_mix:" + std::to_string(params.dry_mix));

        if (effect.effect_type == "compressor") {
            param_parts.push_back("threshold:" + std::to_string(params.threshold));
            param_parts.push_back("ratio:" + std::to_string(params.ratio));
        } else if (effect.effect_type == "reverb") {
            param_parts.push_back("decay:" + std::to_string(params.decay));
        } else if (effect.effect_type == "delay") {
            param_parts.push_back("feedback:" + std::to_string(params.feedback));
        }

        for (size_t i = 0; i < param_parts.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << param_parts[i];
        }
        oss << "}";
    }

    oss << ")";
    return oss.str();
}

std::string EffectAgent::getTrackIdFromContext(const nlohmann::json& context) const {
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

} // namespace magda
