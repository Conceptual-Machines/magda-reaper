#include "magda_cpp/agents/volume_agent.h"
#include "prompt_loader.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace magda {

VolumeAgent::VolumeAgent(const std::string& api_key)
    : BaseAgent("volume", api_key) {}

bool VolumeAgent::canHandle(const std::string& operation) const {
    std::string op_lower = operation;
    std::transform(op_lower.begin(), op_lower.end(), op_lower.begin(), ::tolower);

    return op_lower.find("volume") != std::string::npos ||
           op_lower.find("pan") != std::string::npos ||
           op_lower.find("mute") != std::string::npos ||
           op_lower.find("set") != std::string::npos;
}

AgentResponse VolumeAgent::execute(const std::string& operation,
                                   const nlohmann::json& context) {
    // Parse the operation with LLM
    std::cout << "🔍 DEBUG: Volume agent executing operation: " << operation << std::endl;

    // Load shared prompt
    static SharedResources resources;
    std::string instructions = resources.getVolumeAgentPrompt();

    // Create a structured schema for volume parameters
    auto schema = JsonSchemaBuilder()
        .type("object")
        .title("Volume Parameters")
        .description("Parameters for controlling volume, pan, and mute in a DAW")
        .property("track_name", JsonSchemaBuilder()
            .type("string")
            .description("The name of the track to control"))
        .property("volume", JsonSchemaBuilder()
            .type("number")
            .description("Volume level (0.0 to 1.0, or percentage 0-100)")
            .minimum(-100.0)
            .maximum(100.0))
        .property("pan", JsonSchemaBuilder()
            .type("number")
            .description("Pan position (-1.0 to 1.0, where -1 is left, 0 is center, 1 is right)"))
        .property("mute", JsonSchemaBuilder()
            .type("boolean")
            .description("Mute state (true for mute, false for unmute)"))
        .required({"track_name", "volume", "pan", "mute"})
        .additionalProperties(false);

    auto volume_info = parseOperationWithLLM(operation, instructions, schema);
    std::cout << "🔍 DEBUG: Volume agent parsed info: " << volume_info.dump(2) << std::endl;

    // Get track information from context
    std::string track_id = getTrackIdFromContext(context);
    if (track_id.empty()) {
        track_id = generateUniqueId();
    }

    std::string track_name = getTrackNameFromContext(context);
    if (track_name == "unknown") {
        track_name = volume_info.value("track_name", "track_" + track_id.substr(0, 8));
    }

    // Create volume result
    VolumeResult volume_result(track_name, track_id);

    // Set volume if provided
    if (volume_info.contains("volume")) {
        volume_result.volume = volume_info["volume"].get<float>();
    }

    // Set pan if provided
    if (volume_info.contains("pan")) {
        volume_result.pan = volume_info["pan"].get<float>();
    }

    // Set mute if provided
    if (volume_info.contains("mute")) {
        volume_result.mute = volume_info["mute"].get<bool>();
    }

    // Store volume settings for future reference
    volume_settings_[track_id] = volume_result.toJson();

    // Generate DAW command
    std::string daw_command = generateDAWCommand(volume_result.toJson());

    return AgentResponse(volume_result.toJson(), daw_command, context);
}

std::vector<std::string> VolumeAgent::getCapabilities() const {
    return {"volume", "pan", "mute", "set volume"};
}

nlohmann::json VolumeAgent::getVolumeSettings(const std::string& track_id) const {
    auto it = volume_settings_.find(track_id);
    if (it != volume_settings_.end()) {
        return it->second;
    }
    return nlohmann::json::object();
}

std::vector<nlohmann::json> VolumeAgent::listVolumeSettings() const {
    std::vector<nlohmann::json> settings;
    settings.reserve(volume_settings_.size());

    for (const auto& [id, setting] : volume_settings_) {
        settings.push_back(setting);
    }

    return settings;
}

std::string VolumeAgent::generateDAWCommand(const nlohmann::json& result) const {
    std::ostringstream oss;
    oss << "volume(";

    bool first = true;

    if (result.contains("track_name") && !result["track_name"].get<std::string>().empty()) {
        if (!first) oss << ", ";
        oss << "track:" << result["track_name"].get<std::string>();
        first = false;
    }

    if (result.contains("volume")) {
        if (!first) oss << ", ";
        oss << "level:" << result["volume"].get<float>();
        first = false;
    }

    if (result.contains("pan")) {
        if (!first) oss << ", ";
        oss << "pan:" << result["pan"].get<float>();
        first = false;
    }

    if (result.contains("mute")) {
        if (!first) oss << ", ";
        oss << "mute:" << (result["mute"].get<bool>() ? "true" : "false");
    }

    oss << ")";
    return oss.str();
}

} // namespace magda
