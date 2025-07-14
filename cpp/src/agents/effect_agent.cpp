#include "magda_cpp/agents/effect_agent.h"
#include "prompt_loader.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace magda {

EffectAgent::EffectAgent(const std::string& api_key)
    : BaseAgent("effect", api_key) {}

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
        // Load shared prompt
        SharedResources resources;
        std::string prompt = resources.getEffectAgentPrompt();

        // Create schema for effect parameters
        auto schema = JsonSchemaBuilder()
            .type("object")
            .title("Effect Parameters")
            .description("Parameters for adding effects in a DAW")
            .property("effect_type", JsonSchemaBuilder()
                .type("string")
                .description("The type of effect (reverb, delay, compressor, eq, filter, distortion, etc.)"))
            .property("parameters", JsonSchemaBuilder()
                .type("object")
                .description("A dictionary of effect parameters")
                .additionalProperties(false)
                .required({}))
            .property("position", JsonSchemaBuilder()
                .type("string")
                .description("Where to insert the effect (insert, send, master)"))
            .required({"effect_type", "position"})
            .additionalProperties(false);

        // Parse effect operation using base class method
        auto effect_info = parseOperationWithLLM(operation, prompt, schema);

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

std::string EffectAgent::generateDAWCommand(const nlohmann::json& result) const {
    std::ostringstream oss;
    oss << "effect(track:" << result.value("track_id", "unknown")
        << ", type:" << result.value("effect_type", "reverb")
        << ", position:" << result.value("position", "insert");

    if (result.contains("parameters") && result["parameters"].is_object()) {
        const auto& params = result["parameters"];
        oss << ", params:{";

        std::vector<std::string> param_parts;
        if (params.contains("wet_mix")) {
            param_parts.push_back("wet_mix:" + std::to_string(params["wet_mix"].get<double>()));
        }
        if (params.contains("dry_mix")) {
            param_parts.push_back("dry_mix:" + std::to_string(params["dry_mix"].get<double>()));
        }
        if (params.contains("threshold")) {
            param_parts.push_back("threshold:" + std::to_string(params["threshold"].get<double>()));
        }
        if (params.contains("ratio")) {
            param_parts.push_back("ratio:" + std::to_string(params["ratio"].get<double>()));
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
