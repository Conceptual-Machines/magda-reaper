#include "magda_cpp/models.h"
#include <sstream>

namespace magda {

// TrackResult implementation
TrackResult::TrackResult(const std::string& id, const std::string& name, 
                         const std::optional<std::string>& vst_plugin)
    : track_id(id), track_name(name), vst(vst_plugin), instrument(vst_plugin) {}

nlohmann::json TrackResult::toJson() const {
    nlohmann::json j;
    j["track_id"] = track_id;
    j["track_name"] = track_name;
    if (vst) j["vst"] = *vst;
    j["type"] = type;
    if (instrument) j["instrument"] = *instrument;
    return j;
}

// ClipResult implementation
ClipResult::ClipResult(const std::string& id, const std::string& track_name, 
                       const std::string& track_id, int start, int end)
    : clip_id(id), track_name(track_name), track_id(track_id), 
      start_bar(start), end_bar(end) {}

nlohmann::json ClipResult::toJson() const {
    nlohmann::json j;
    j["clip_id"] = clip_id;
    j["track_name"] = track_name;
    j["track_id"] = track_id;
    if (start_time) j["start_time"] = *start_time;
    if (duration) j["duration"] = *duration;
    j["start_bar"] = start_bar;
    j["end_bar"] = end_bar;
    return j;
}

// VolumeResult implementation
VolumeResult::VolumeResult(const std::string& track_name, const std::string& track_id, float vol)
    : track_name(track_name), track_id(track_id), volume(vol) {}

nlohmann::json VolumeResult::toJson() const {
    nlohmann::json j;
    j["track_name"] = track_name;
    j["track_id"] = track_id;
    j["volume"] = volume;
    if (pan) j["pan"] = *pan;
    if (mute) j["mute"] = *mute;
    return j;
}

// EffectParameters implementation
nlohmann::json EffectParameters::toJson() const {
    nlohmann::json j;
    j["wet_mix"] = wet_mix;
    j["dry_mix"] = dry_mix;
    j["threshold"] = threshold;
    j["ratio"] = ratio;
    j["attack"] = attack;
    j["release"] = release;
    j["decay"] = decay;
    j["feedback"] = feedback;
    j["delay_time"] = delay_time;
    j["frequency"] = frequency;
    j["q_factor"] = q_factor;
    j["gain"] = gain;
    j["wet"] = wet;
    j["dry"] = dry;
    return j;
}

// EffectResult implementation
EffectResult::EffectResult(const std::string& track_name, const std::string& track_id,
                           const std::string& type, const std::string& pos)
    : track_name(track_name), track_id(track_id), effect_type(type), position(pos) {}

nlohmann::json EffectResult::toJson() const {
    nlohmann::json j;
    j["track_name"] = track_name;
    j["track_id"] = track_id;
    j["effect_type"] = effect_type;
    if (parameters) j["parameters"] = parameters->toJson();
    j["position"] = position;
    return j;
}

// MIDIResult implementation
MIDIResult::MIDIResult(const std::string& track_name, const std::string& track_id,
                       const std::string& note_val, int vel)
    : track_name(track_name), track_id(track_id), velocity(vel), note(note_val) {}

nlohmann::json MIDIResult::toJson() const {
    nlohmann::json j;
    j["track_name"] = track_name;
    j["track_id"] = track_id;
    j["operation"] = operation;
    if (quantization) j["quantization"] = *quantization;
    if (transpose_semitones) j["transpose_semitones"] = *transpose_semitones;
    j["velocity"] = velocity;
    j["note"] = note;
    j["duration"] = duration;
    j["start_bar"] = start_bar;
    j["channel"] = channel;
    return j;
}

// Operation implementation
Operation::Operation(OperationType type, const std::map<std::string, std::string>& params,
                     const std::string& agent)
    : operation_type(type), parameters(params), agent_name(agent) {}

std::string Operation::toString() const {
    std::ostringstream oss;
    oss << "Operation{type=";
    
    switch (operation_type) {
        case OperationType::CREATE_TRACK: oss << "CREATE_TRACK"; break;
        case OperationType::CREATE_CLIP: oss << "CREATE_CLIP"; break;
        case OperationType::SET_VOLUME: oss << "SET_VOLUME"; break;
        case OperationType::ADD_EFFECT: oss << "ADD_EFFECT"; break;
        case OperationType::CREATE_MIDI: oss << "CREATE_MIDI"; break;
        default: oss << "UNKNOWN"; break;
    }
    
    oss << ", parameters={";
    bool first = true;
    for (const auto& [key, value] : parameters) {
        if (!first) oss << ", ";
        oss << key << "=" << value;
        first = false;
    }
    oss << "}, agent=" << agent_name << "}";
    
    return oss.str();
}

// AgentResponse implementation
AgentResponse::AgentResponse(const nlohmann::json& res, const std::string& command,
                             const nlohmann::json& ctx)
    : result(res), daw_command(command), context(ctx) {}

nlohmann::json AgentResponse::toJson() const {
    nlohmann::json j;
    j["result"] = result;
    j["daw_command"] = daw_command;
    j["context"] = context;
    return j;
}

} // namespace magda 