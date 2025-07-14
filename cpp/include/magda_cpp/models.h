#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>
#include "openai/OpenAITypes.h"

namespace magda {

/**
 * @brief Base class for all MAGDA result types
 */
class BaseResult {
public:
    virtual ~BaseResult() = default;
    virtual nlohmann::json toJson() const = 0;
};

/**
 * @brief Track creation result
 */
class TrackResult : public BaseResult {
public:
    std::string track_id;
    std::string track_name;
    std::optional<std::string> vst;
    std::string type = "track";
    std::optional<std::string> instrument;

    TrackResult() = default;
    TrackResult(const std::string& id, const std::string& name,
                const std::optional<std::string>& vst_plugin = std::nullopt);

    nlohmann::json toJson() const override;
};

/**
 * @brief Clip creation result
 */
class ClipResult : public BaseResult {
public:
    std::string clip_id;
    std::string track_name;
    std::string track_id;
    std::optional<double> start_time;
    std::optional<double> duration;
    int start_bar = 1;
    int end_bar = 4;

    ClipResult() = default;
    ClipResult(const std::string& id, const std::string& track_name,
               const std::string& track_id, int start = 1, int end = 4);

    nlohmann::json toJson() const override;
};

/**
 * @brief Volume control result
 */
class VolumeResult : public BaseResult {
public:
    std::string track_name;
    std::string track_id;
    float volume = 0.0f;
    std::optional<float> pan;
    std::optional<bool> mute;

    VolumeResult() = default;
    VolumeResult(const std::string& track_name, const std::string& track_id,
                 float vol = 0.0f);

    nlohmann::json toJson() const override;
};

/**
 * @brief Effect parameters
 */
class EffectParameters {
public:
    float wet_mix = 0.5f;
    float dry_mix = 0.5f;
    float threshold = -20.0f;
    float ratio = 4.0f;
    float attack = 0.01f;
    float release = 0.1f;
    float decay = 0.5f;
    float feedback = 0.3f;
    float delay_time = 0.5f;
    float frequency = 1000.0f;
    float q_factor = 1.0f;
    float gain = 0.0f;
    float wet = 0.5f;
    float dry = 0.5f;

    nlohmann::json toJson() const;
};

/**
 * @brief Effect result
 */
class EffectResult : public BaseResult {
public:
    std::string track_name;
    std::string track_id;
    std::string effect_type;
    std::optional<EffectParameters> parameters;
    std::string position = "insert";

    EffectResult() = default;
    EffectResult(const std::string& track_name, const std::string& track_id,
                 const std::string& type, const std::string& pos = "insert");

    nlohmann::json toJson() const override;
};

/**
 * @brief MIDI result
 */
class MIDIResult : public BaseResult {
public:
    std::string track_name;
    std::string track_id;
    std::string operation = "note";
    std::optional<std::string> quantization;
    std::optional<int> transpose_semitones;
    int velocity = 100;
    std::string note = "C4";
    double duration = 1.0;
    int start_bar = 1;
    int channel = 1;

    MIDIResult() = default;
    MIDIResult(const std::string& track_name, const std::string& track_id,
               const std::string& note_val = "C4", int vel = 100);

    nlohmann::json toJson() const override;
};

/**
 * @brief Operation types enum
 */
enum class OperationType {
    CREATE_TRACK,
    CREATE_CLIP,
    SET_VOLUME,
    ADD_EFFECT,
    CREATE_MIDI,
    UNKNOWN
};

/**
 * @brief Operation structure
 */
class Operation {
public:
    OperationType operation_type;
    std::map<std::string, std::string> parameters;
    std::string agent_name;

    Operation() = default;
    Operation(OperationType type, const std::map<std::string, std::string>& params,
              const std::string& agent = "");

    std::string toString() const;
};

/**
 * @brief Agent response structure
 */
class AgentResponse {
public:
    nlohmann::json result;
    std::string daw_command;
    nlohmann::json context;

    AgentResponse() = default;
    AgentResponse(const nlohmann::json& res, const std::string& command,
                  const nlohmann::json& ctx = nlohmann::json::object());

    nlohmann::json toJson() const;
};

/**
 * @brief Centralized model configuration for MAGDA agents
 *
 * This ensures consistency across all agents and makes it easy to update
 * model choices for different stages of the pipeline.
 */
struct ModelConfig {
    // First stage: Operation orchestration (cost-effective, fast)
    static constexpr OpenAI::Model ORCHESTRATOR_AGENT = OpenAI::Model::GPT_4_1_Nano;

    // Second stage: Specialized agents (higher quality, structured output)
    static constexpr OpenAI::Model SPECIALIZED_AGENTS = OpenAI::Model::GPT_4_1;

    // Alternative for specialized agents (cost-effective but still high quality)
    static constexpr OpenAI::Model SPECIALIZED_AGENTS_MINI = OpenAI::Model::GPT_4_1_Mini;

    // Fallback model for error cases
    static constexpr OpenAI::Model FALLBACK = OpenAI::Model::GPT_4o_Mini;

    // Current model choices (easy to change in one place)
    static constexpr OpenAI::Model CURRENT_DECISION_AGENT = OpenAI::Model::GPT_4_1_Mini;
    static constexpr OpenAI::Model CURRENT_SPECIALIZED_AGENTS = OpenAI::Model::GPT_4o_Mini;

    // Convenience methods for common use cases
    static std::string getOrchestratorAgentModel() {
        return OpenAI::toString(ORCHESTRATOR_AGENT);
    }

    static std::string getSpecializedAgentModel() {
        return OpenAI::toString(SPECIALIZED_AGENTS);
    }

    static std::string getFallbackModel() {
        return OpenAI::toString(FALLBACK);
    }


};

} // namespace magda
