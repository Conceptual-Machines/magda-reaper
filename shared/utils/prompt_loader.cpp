#include "prompt_loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace magda {

SharedResources::SharedResources(const std::string& base_path) {
    base_path_ = base_path.empty() ? findSharedResourcesPath() : std::filesystem::path(base_path);
    loadPrompts();
    loadSchemas();
}

std::string SharedResources::getOperationIdentifierPrompt() const {
    return operation_identifier_prompt_;
}

std::string SharedResources::getTrackAgentPrompt() const {
    return track_agent_prompt_;
}

std::string SharedResources::getEffectAgentPrompt() const {
    return effect_agent_prompt_;
}

std::string SharedResources::getVolumeAgentPrompt() const {
    return volume_agent_prompt_;
}

std::string SharedResources::getMidiAgentPrompt() const {
    return midi_agent_prompt_;
}

std::string SharedResources::getClipAgentPrompt() const {
    return clip_agent_prompt_;
}

nlohmann::json SharedResources::getDawOperationSchema() const {
    return daw_operation_schema_;
}

std::string SharedResources::loadPrompt(const std::string& prompt_name) const {
    return loadPromptFile(prompt_name);
}

nlohmann::json SharedResources::loadSchema(const std::string& schema_name) const {
    std::string schema_path = (base_path_ / "schemas" / (schema_name + ".json")).string();
    if (std::filesystem::exists(schema_path)) {
        std::ifstream file(schema_path);
        if (file.is_open()) {
            try {
                nlohmann::json schema;
                file >> schema;
                return schema;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing schema file: " << e.what() << std::endl;
            }
        }
    }
    return nlohmann::json::object();
}

void SharedResources::loadPrompts() {
    // Load operation identifier prompt
    operation_identifier_prompt_ = loadPromptFile("operation_identifier");

    // Load agent prompts
    track_agent_prompt_ = loadPromptFile("track_agent");
    effect_agent_prompt_ = loadPromptFile("effect_agent");
    volume_agent_prompt_ = loadPromptFile("volume_agent");
    midi_agent_prompt_ = loadPromptFile("midi_agent");
    clip_agent_prompt_ = loadPromptFile("clip_agent");
}

std::string SharedResources::loadPromptFile(const std::string& prompt_name) const {
    std::string prompt_path = (base_path_ / "prompts" / (prompt_name + ".md")).string();
    if (std::filesystem::exists(prompt_path)) {
        std::ifstream file(prompt_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::cout << "Loaded shared prompt from: " << prompt_path << std::endl;
            return buffer.str();
        }
    }

    // Return fallback prompts based on agent type
    if (prompt_name == "operation_identifier") {
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
    } else if (prompt_name == "track_agent") {
        return R"(
You are a track creation specialist for a DAW system.
Your job is to parse track creation requests and extract the necessary parameters.

Extract the following information:
- vst: The VST plugin name (e.g., "serum", "addictive drums")
- name: The track name (e.g., "bass", "drums")
- type: Track type (usually "audio" or "midi")

Return a JSON object with the extracted parameters following the provided schema.
)";
    } else if (prompt_name == "effect_agent") {
        return R"(
You are an effect specialist for a DAW system.
Your job is to parse effect requests and extract the necessary parameters.

Extract the following information:
- effect_type: The type of effect (reverb, delay, compressor, eq, filter, distortion, etc.)
- parameters: A dictionary of effect parameters (e.g., {"wet": 0.5, "decay": 2.0})
- position: Where to insert the effect (insert, send, master, default: insert)

Return a JSON object with the extracted parameters following the provided schema.
)";
    } else if (prompt_name == "volume_agent") {
        return R"(
You are a volume automation specialist for a DAW system.
Your job is to parse volume automation requests and extract the necessary parameters.

Extract the following information:
- start_value: The starting volume value (0.0 to 1.0, default: 0.0)
- end_value: The ending volume value (0.0 to 1.0, default: 1.0)
- start_bar: The starting bar number (default: 1)
- end_bar: The ending bar number (default: start_bar + 4)

Return a JSON object with the extracted parameters following the provided schema.
)";
    } else if (prompt_name == "midi_agent") {
        return R"(
You are a MIDI specialist for a DAW system.
Your job is to parse MIDI requests and extract the necessary parameters.

Extract the following information:
- operation: The type of MIDI operation (note, chord, quantize, transpose, etc.)
- note: The MIDI note (e.g., "C4", "A#3", default: "C4")
- velocity: Note velocity (0-127, default: 100)
- duration: Note duration in seconds (default: 1.0)
- start_bar: Starting bar number (default: 1)
- channel: MIDI channel (1-16, default: 1)
- quantization: Quantization value if specified
- transpose_semitones: Transpose amount in semitones if specified

Return a JSON object with the extracted parameters following the provided schema.
)";
    } else if (prompt_name == "clip_agent") {
        return R"(
You are a clip specialist for a DAW system.
Your job is to parse clip requests and extract the necessary parameters.

Extract the following information:
- start_bar: Starting bar number (default: 1)
- end_bar: Ending bar number (default: start_bar + 4)
- start_time: Start time in seconds (optional)
- duration: Clip duration in seconds (optional)
- track_name: Target track name (optional)

Return a JSON object with the extracted parameters following the provided schema.
)";
    }

    std::cout << "Using fallback hardcoded prompt for: " << prompt_name << std::endl;
    return "Fallback prompt for " + prompt_name;
}

void SharedResources::loadSchemas() {
    // Try to load DAW operation schema
    std::string schema_path = (base_path_ / "schemas" / "daw_operation.json").string();
    if (std::filesystem::exists(schema_path)) {
        std::ifstream file(schema_path);
        if (file.is_open()) {
            try {
                file >> daw_operation_schema_;
                std::cout << "Loaded shared schema from: " << schema_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing schema file: " << e.what() << std::endl;
                loadDefaultSchema();
            }
        }
    } else {
        loadDefaultSchema();
    }
}

void SharedResources::loadDefaultSchema() {
    // Fallback to hardcoded schema
    daw_operation_schema_ = {
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
    std::cout << "Using fallback hardcoded schema" << std::endl;
}

std::filesystem::path SharedResources::findSharedResourcesPath() {
    // Try to find the shared resources directory
    std::vector<std::filesystem::path> possible_paths = {
        std::filesystem::current_path() / "shared",
        std::filesystem::current_path() / ".." / "shared",
        std::filesystem::current_path() / ".." / ".." / "shared",
        std::filesystem::current_path() / ".." / ".." / ".." / "shared"
    };

    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            return path;
        }
    }

    // Default fallback
    return std::filesystem::current_path() / "shared";
}

} // namespace magda
