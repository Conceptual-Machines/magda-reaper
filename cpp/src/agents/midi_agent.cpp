#include "magda_cpp/agents/midi_agent.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace magda {

MidiAgent::MidiAgent(const std::string& api_key) {
    // Initialize OpenAI client
    OpenAI::Config config;
    if (!api_key.empty()) {
        config.api_key = api_key;
    }
    client_ = std::make_unique<OpenAI::OpenAIClient>(config);
}

bool MidiAgent::canHandle(const std::string& operation) const {
    std::string op_lower = operation;
    std::transform(op_lower.begin(), op_lower.end(), op_lower.begin(), ::tolower);

    return op_lower.find("midi") != std::string::npos ||
           op_lower.find("note") != std::string::npos ||
           op_lower.find("chord") != std::string::npos ||
           op_lower.find("quantize") != std::string::npos ||
           op_lower.find("transpose") != std::string::npos;
}

AgentResponse MidiAgent::execute(const std::string& operation, const nlohmann::json& context) {
    try {
        // Parse MIDI operation using LLM
        auto midi_info = parseMidiOperationWithLLM(operation);

        // Get track ID from context
        std::string track_id = getTrackIdFromContext(context);

        // Generate unique MIDI event ID
        std::string midi_id = generateUniqueId();

        // Create MIDI result
        MIDIResult midi_result;
        midi_result.track_id = track_id;
        midi_result.operation = midi_info.value("operation", "note");
        midi_result.note = midi_info.value("note", "C4");
        midi_result.velocity = midi_info.value("velocity", 100);
        midi_result.duration = midi_info.value("duration", 1.0);
        midi_result.start_bar = midi_info.value("start_bar", 1);
        midi_result.channel = midi_info.value("channel", 1);

        // Handle quantization and transpose if specified
        if (midi_info.contains("quantization")) {
            midi_result.quantization = midi_info["quantization"];
        }
        if (midi_info.contains("transpose_semitones")) {
            midi_result.transpose_semitones = midi_info["transpose_semitones"];
        }

        // Store MIDI event for future reference
        midi_events_[midi_id] = midi_result;

        // Generate DAW command
        std::string daw_command = generateDawCommand(midi_result);

        // Create result JSON
        nlohmann::json result;
        result["id"] = midi_id;
        result["track_id"] = track_id;
        result["operation"] = midi_result.operation;
        result["note"] = midi_result.note;
        result["velocity"] = midi_result.velocity;
        result["duration"] = midi_result.duration;
        result["start_bar"] = midi_result.start_bar;
        result["channel"] = midi_result.channel;

        if (midi_result.quantization.has_value()) {
            result["quantization"] = midi_result.quantization.value();
        }
        if (midi_result.transpose_semitones.has_value()) {
            result["transpose_semitones"] = midi_result.transpose_semitones.value();
        }

        return AgentResponse(result, daw_command, context);

    } catch (const std::exception& e) {
        nlohmann::json error_result;
        error_result["error"] = "Error executing MIDI operation: " + std::string(e.what());
        return AgentResponse(error_result, "", context);
    }
}

std::vector<std::string> MidiAgent::getCapabilities() const {
    return {"midi", "note", "chord", "quantize", "transpose"};
}

std::optional<MIDIResult> MidiAgent::getMidiById(const std::string& midi_id) const {
    auto it = midi_events_.find(midi_id);
    if (it != midi_events_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<MIDIResult> MidiAgent::listMidiEvents() const {
    std::vector<MIDIResult> result;
    for (const auto& pair : midi_events_) {
        result.push_back(pair.second);
    }
    return result;
}

nlohmann::json MidiAgent::parseMidiOperationWithLLM(const std::string& operation) {
    try {
        LLMRequest request;
        request.model = OpenAI::Model::GPT_4o_Mini;
        request.system_prompt = R"(
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

Return a JSON object with the extracted parameters.
)";
        request.user_prompt = operation;
        request.temperature = 0.1f;
        request.max_tokens = 500;

        // Add JSON schema for structured output
        request.json_schema = {
            {"type", "object"},
            {"properties", {
                {"operation", {{"type", "string"}}},
                {"note", {{"type", "string"}}},
                {"velocity", {{"type", "integer"}}},
                {"duration", {{"type", "number"}}},
                {"start_bar", {{"type", "integer"}}},
                {"channel", {{"type", "integer"}}},
                {"quantization", {{"type", "string"}}},
                {"transpose_semitones", {{"type", "integer"}}}
            }},
            {"required", {"operation"}}
        };

        auto response = client_->chat(request);

        if (response.success && response.json_content.has_value()) {
            return response.json_content.value();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing MIDI operation: " << e.what() << std::endl;
    }

    // Return default values if parsing fails
    return {
        {"operation", "note"},
        {"note", "C4"},
        {"velocity", 100},
        {"duration", 1.0},
        {"start_bar", 1},
        {"channel", 1}
    };
}

std::string MidiAgent::generateDawCommand(const MIDIResult& midi) const {
    std::ostringstream oss;
    oss << "midi(track:" << midi.track_id
        << ", operation:" << midi.operation
        << ", note:" << midi.note
        << ", velocity:" << midi.velocity
        << ", duration:" << midi.duration
        << ", start_bar:" << midi.start_bar
        << ", channel:" << midi.channel;

    if (midi.quantization.has_value()) {
        oss << ", quantization:" << midi.quantization.value();
    }
    if (midi.transpose_semitones.has_value()) {
        oss << ", transpose:" << midi.transpose_semitones.value();
    }

    oss << ")";
    return oss.str();
}

std::string MidiAgent::getTrackIdFromContext(const nlohmann::json& context) const {
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
