#include "magda_cpp/agents/track_agent.h"
#include "prompt_loader.hpp"
#include <algorithm>
#include <sstream>

namespace magda {

TrackAgent::TrackAgent(const std::string& api_key)
    : BaseAgent("track", api_key) {}

bool TrackAgent::canHandle(const std::string& operation) const {
    std::string op_lower = operation;
    std::transform(op_lower.begin(), op_lower.end(), op_lower.begin(), ::tolower);

    return op_lower.find("track") != std::string::npos ||
           op_lower.find("create track") != std::string::npos ||
           op_lower.find("add track") != std::string::npos;
}

AgentResponse TrackAgent::execute(const std::string& operation,
                                  const nlohmann::json& context) {
    // Parse the operation with LLM
    auto track_info = parseTrackOperationWithLLM(operation);

    // Get track information from context
    std::string track_id = getTrackIdFromContext(context);
    if (track_id.empty()) {
        track_id = generateUniqueId();
    }

    std::string track_name = getTrackNameFromContext(context);
    if (track_name == "unknown") {
        track_name = track_info.value("name", "track_" + track_id.substr(0, 8));
    }

    // Create track result
    TrackResult track_result(
        track_id,
        track_name,
        track_info.value("vst", std::string())
    );

    // Store track for future reference
    created_tracks_[track_id] = track_result.toJson();

    // Generate DAW command
    std::string daw_command = generateDAWCommand(track_result.toJson());

    return AgentResponse(track_result.toJson(), daw_command, context);
}

std::vector<std::string> TrackAgent::getCapabilities() const {
    return {"track", "create track", "add track"};
}

nlohmann::json TrackAgent::getTrackById(const std::string& track_id) const {
    auto it = created_tracks_.find(track_id);
    if (it != created_tracks_.end()) {
        return it->second;
    }
    return nlohmann::json::object();
}

std::vector<nlohmann::json> TrackAgent::listTracks() const {
    std::vector<nlohmann::json> tracks;
    tracks.reserve(created_tracks_.size());

    for (const auto& [id, track] : created_tracks_) {
        tracks.push_back(track);
    }

    return tracks;
}

nlohmann::json TrackAgent::parseTrackOperationWithLLM(const std::string& operation) {
    // Load shared prompt
    std::string instructions;
    try {
        auto& resources = shared::getSharedResources();
        instructions = resources.getTrackAgentPrompt();
    } catch (...) {
        // Fallback to hardcoded prompt
        instructions = R"(
You are a track creation specialist for a DAW system.
Your job is to parse track creation requests and extract the necessary parameters.

Extract the following information:
- name: The track name (e.g., "bass", "drums", "lead")
- vst: The VST plugin name (e.g., "serum", "addictive drums", "kontakt")
- type: Track type (usually "audio" or "midi")

Return a JSON object with the extracted parameters following the provided schema.
)";
    }

    // Create a structured schema for track parameters
    auto schema = llmcpp::JsonSchemaBuilder()
        .type("object")
        .title("Track Parameters")
        .description("Parameters for creating a track in a DAW")
        .property("name", llmcpp::JsonSchemaBuilder()
            .type("string")
            .description("The name of the track (e.g., 'bass', 'drums', 'lead')")
            .required())
        .property("vst", llmcpp::JsonSchemaBuilder()
            .type("string")
            .description("The VST plugin name (e.g., 'serum', 'addictive drums', 'kontakt')")
            .optionalString())
        .property("type", llmcpp::JsonSchemaBuilder()
            .type("string")
            .description("Track type")
            .enumValues({"audio", "midi"})
            .defaultValue("midi"))
        .required({"name"});

    return parseOperationWithLLM(operation, instructions, schema);
}

std::string TrackAgent::generateDAWCommand(const nlohmann::json& result) const {
    std::ostringstream oss;
    oss << "track(";

    bool first = true;

    if (result.contains("track_name") && !result["track_name"].get<std::string>().empty()) {
        if (!first) oss << ", ";
        oss << "name:" << result["track_name"].get<std::string>();
        first = false;
    }

    if (result.contains("vst") && !result["vst"].is_null()) {
        if (!first) oss << ", ";
        oss << "vst:" << result["vst"].get<std::string>();
        first = false;
    }

    if (result.contains("track_id") && !result["track_id"].get<std::string>().empty()) {
        if (!first) oss << ", ";
        oss << "id:" << result["track_id"].get<std::string>();
    }

    oss << ")";
    return oss.str();
}

} // namespace magda
