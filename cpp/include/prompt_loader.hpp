#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace magda {

/**
 * @brief Load and manage shared prompts and schemas for C++ implementation
 *
 * This class supports both file-based loading and binary data loading for efficiency.
 * Binary data loading is preferred for production builds as it eliminates runtime file I/O.
 */
class SharedResources {
public:
    /**
     * @brief Constructor
     * @param base_path Path to shared resources directory (optional)
     * @param use_binary_data Whether to use embedded binary data instead of file I/O
     */
    explicit SharedResources(const std::string& base_path = "", bool use_binary_data = true);

    /**
     * @brief Load a prompt from the shared prompts directory or binary data
     * @param prompt_name Name of the prompt file (without .md extension)
     * @return The prompt content as a string
     */
    std::string loadPrompt(const std::string& prompt_name) const;

    /**
     * @brief Load a JSON schema from the shared schemas directory or binary data
     * @param schema_name Name of the schema file (without .json extension)
     * @return The schema as a JSON object
     */
    nlohmann::json loadSchema(const std::string& schema_name) const;

    /**
     * @brief Get the orchestrator agent system prompt
     * @return The orchestrator agent prompt
     */
    std::string getOrchestratorAgentPrompt() const;

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
    bool use_binary_data_;

    // Cached prompts
    std::string orchestrator_agent_prompt_;
    std::string track_agent_prompt_;
    std::string effect_agent_prompt_;
    std::string volume_agent_prompt_;
    std::string midi_agent_prompt_;
    std::string clip_agent_prompt_;

    // Cached schemas
    nlohmann::json daw_operation_schema_;

    /**
     * @brief Load a prompt file from the shared prompts directory
     * @param prompt_name Name of the prompt file (without .md extension)
     * @return The prompt content as a string
     */
    std::string loadPromptFile(const std::string& prompt_name) const;

    /**
     * @brief Load a prompt from embedded binary data
     * @param prompt_name Name of the prompt
     * @return The prompt content as a string
     */
    std::string loadPromptFromBinary(const std::string& prompt_name) const;

    /**
     * @brief Load a schema from embedded binary data
     * @param schema_name Name of the schema
     * @return The schema as a JSON object
     */
    nlohmann::json loadSchemaFromBinary(const std::string& schema_name) const;

    /**
     * @brief Load prompts into cache
     */
    void loadPrompts();

    /**
     * @brief Load schemas into cache
     */
    void loadSchemas();

    /**
     * @brief Load default schema if file not found
     */
    void loadDefaultSchema();

    /**
     * @brief Find the shared resources directory
     * @return Path to shared resources
     */
    static std::filesystem::path findSharedResourcesPath();
};

} // namespace magda
