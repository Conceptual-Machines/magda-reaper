#include "prompt_loader.hpp"
#include "binary_data.h"
#include <core/JsonSchemaBuilder.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace magda {

SharedResources::SharedResources(const std::string& base_path, bool use_binary_data) {
    base_path_ = base_path.empty() ? findSharedResourcesPath() : std::filesystem::path(base_path);
    use_binary_data_ = use_binary_data;
    loadPrompts();
    loadSchemas();
}

std::string SharedResources::getOrchestratorAgentPrompt() const {
    return orchestrator_agent_prompt_;
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
    if (use_binary_data_) {
        return loadPromptFromBinary(prompt_name);
    }
    return loadPromptFile(prompt_name);
}

nlohmann::json SharedResources::loadSchema(const std::string& schema_name) const {
    // Temporarily disable binary data for schemas due to JSON parsing issues
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

std::string SharedResources::loadPromptFromBinary(const std::string& prompt_name) const {
    const BinaryData* data = getBinaryData(prompt_name);
    if (data) {
        std::cout << "Loaded binary prompt: " << prompt_name << std::endl;
        return binaryDataToString(data);
    }

    throw std::runtime_error("Binary data not available for prompt: " + prompt_name + ". This indicates a build configuration issue.");
}

nlohmann::json SharedResources::loadSchemaFromBinary(const std::string& schema_name) const {
    const BinaryData* data = getBinaryData(schema_name);
    if (data) {
        std::cout << "Loaded binary schema: " << schema_name << std::endl;
        std::string schema_str = binaryDataToString(data);
        try {
            return nlohmann::json::parse(schema_str);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing binary schema: " << e.what() << std::endl;
        }
    }

    throw std::runtime_error("Binary data not available for schema: " + schema_name + ". This indicates a build configuration issue.");
}

void SharedResources::loadPrompts() {
    // Load orchestrator agent prompt
    orchestrator_agent_prompt_ = loadPrompt("orchestrator_agent");

    // Load agent prompts
    track_agent_prompt_ = loadPrompt("track_agent");
    effect_agent_prompt_ = loadPrompt("effect_agent");
    volume_agent_prompt_ = loadPrompt("volume_agent");
    midi_agent_prompt_ = loadPrompt("midi_agent");
    clip_agent_prompt_ = loadPrompt("clip_agent");
}

std::string SharedResources::loadPromptFile(const std::string& prompt_name) const {
    std::string prompt_path = (base_path_ / "prompts" / (prompt_name + ".txt")).string();
    if (std::filesystem::exists(prompt_path)) {
        std::ifstream file(prompt_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::cout << "Loaded shared prompt from: " << prompt_path << std::endl;
            return buffer.str();
        }
    }

    throw std::runtime_error("Binary data not available for prompt: " + prompt_name + ". This indicates a build configuration issue.");
}

void SharedResources::loadSchemas() {
    // Try to load DAW operation schema
    daw_operation_schema_ = loadSchema("daw_operation");

    if (daw_operation_schema_.empty()) {
        loadDefaultSchema();
    }
}

void SharedResources::loadDefaultSchema() {
    throw std::runtime_error("Binary data not available for DAW operation schema. This indicates a build configuration issue.");
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
