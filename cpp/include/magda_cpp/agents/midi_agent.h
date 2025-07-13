#pragma once

#include "magda_cpp/agents/base_agent.h"
#include "magda_cpp/models.h"
#include <memory>
#include <string>
#include <map>

namespace magda {

/**
 * @brief Agent responsible for handling MIDI operations using LLM
 *
 * Processes MIDI-related operations like creating notes, chords, quantization,
 * transposition, and other MIDI events in the DAW system.
 */
class MidiAgent : public BaseAgent {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (can be empty to use environment variable)
     */
    explicit MidiAgent(const std::string& api_key = "");

    /**
     * @brief Check if this agent can handle the given operation
     * @param operation The operation string
     * @return True if this agent can handle the operation
     */
    bool canHandle(const std::string& operation) const override;

    /**
     * @brief Execute the MIDI operation
     * @param operation The operation description
     * @param context Optional context information
     * @return AgentResponse with DAW command and result
     */
    AgentResponse execute(const std::string& operation,
                         const nlohmann::json& context = nlohmann::json::object()) override;

    /**
     * @brief Get list of operations this agent can handle
     * @return Vector of supported operation types
     */
    std::vector<std::string> getCapabilities() const override;

    /**
     * @brief Get MIDI event by ID
     * @param midi_id The MIDI event ID
     * @return MIDI result if found, nullopt otherwise
     */
    std::optional<MIDIResult> getMidiById(const std::string& midi_id) const;

    /**
     * @brief List all created MIDI events
     * @return Vector of all MIDI results
     */
    std::vector<MIDIResult> listMidiEvents() const;

private:
    std::map<std::string, MIDIResult> midi_events_;

    /**
     * @brief Generate DAW command from MIDI result
     * @param midi The MIDI result
     * @return DAW command string
     */
    std::string generateDawCommand(const MIDIResult& midi) const;

    /**
     * @brief Generate DAW command from JSON result (BaseAgent interface)
     * @param result The JSON result
     * @return DAW command string
     */
    std::string generateDAWCommand(const nlohmann::json& result) const override;

    /**
     * @brief Get track ID from context
     * @param context The context object
     * @return Track ID string
     */
    std::string getTrackIdFromContext(const nlohmann::json& context) const;
};

} // namespace magda
