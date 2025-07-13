#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace magda_cpp {

/**
 * @brief Load and manage shared prompts and schemas for C++ implementation
 */
class SharedResources {
public:
    /**
     * @brief Constructor
     * @param base_path Path to shared resources directory (optional)
     */
    explicit SharedResources(const std::string& base_path = "");

    /**
     * @brief Load a prompt from the shared prompts directory
     * @param prompt_name Name of the prompt file (without .md extension)
     * @return The prompt content as a string
     */
    std::string loadPrompt(const std::string& prompt_name) const;

    /**
     * @brief Load a JSON schema from the shared schemas directory
     * @param schema_name Name of the schema file (without .json extension)
     * @return The schema as a JSON object
     */
    nlohmann::json loadSchema(const std::string& schema_name) const;

    /**
     * @brief Get the operation identifier system prompt
     * @return The operation identifier prompt
     */
    std::string getOperationIdentifierPrompt() const;

    /**
     * @brief Get the track agent system prompt
     * @return The track agent prompt
     */
    std::string getTrackAgentPrompt() const;

    /**
     * @brief Get the effect agent system prompt
     * @return The effect agent prompt
     */
    std::string getEffectAgentPrompt() const;

    /**
     * @brief Get the volume agent system prompt
     * @return The volume agent prompt
     */
    std::string getVolumeAgentPrompt() const;

    /**
     * @brief Get the MIDI agent system prompt
     * @return The MIDI agent prompt
     */
    std::string getMidiAgentPrompt() const;

    /**
     * @brief Get the clip agent system prompt
     * @return The clip agent prompt
     */
    std::string getClipAgentPrompt() const;

    /**
     * @brief Get the DAW operation JSON schema
     * @return The DAW operation schema
     */
    nlohmann::json getDawOperationSchema() const;

private:
    std::filesystem::path base_path_;

    /**
     * @brief Load a prompt file from the shared prompts directory
     * @param prompt_name Name of the prompt file (without .md extension)
     * @return The prompt content as a string
     */
    std::string loadPromptFile(const std::string& prompt_name) const;

    /**
     * @brief Find the shared resources directory
     * @return Path to shared resources
     */
    static std::filesystem::path findSharedResourcesPath();
};

} // namespace magda_cpp
