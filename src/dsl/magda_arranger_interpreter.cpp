#include "magda_arranger_interpreter.h"
#include "reaper_plugin.h"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
#include <string>

extern reaper_plugin_info_t* g_rec;

namespace MagdaArranger {

// ============================================================================
// Arranger Parameters
// ============================================================================
struct ArrangerParams {
    std::string symbol;      // Chord symbol (Em, C, Am7)
    std::string pitch;       // Note pitch for single notes
    double duration = 4.0;   // Duration in beats
    double length = 4.0;     // Total length in beats
    double noteDuration = 0.25; // Note duration for arpeggios
    double start = 0.0;      // Start position
    int velocity = 100;
    int octave = 3;
    std::string direction = "up";
    std::vector<std::string> chords; // For progressions
};

// ============================================================================
// Implementation
// ============================================================================
Interpreter::Interpreter() : m_targetTrack(nullptr), m_startBeat(0.0) {}
Interpreter::~Interpreter() {}

bool Interpreter::Execute(const char* dsl_code) {
    if (!dsl_code || !*dsl_code) {
        m_error.Set("Empty DSL code");
        return false;
    }

    // Log what we're executing
    if (g_rec) {
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[512];
            snprintf(msg, sizeof(msg), "MAGDA Arranger: Executing: %.200s\n", dsl_code);
            ShowConsoleMsg(msg);
        }
    }

    // Find the call type
    const char* pos = dsl_code;

    // Skip whitespace
    while (*pos && (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t')) pos++;

    if (strncmp(pos, "note(", 5) == 0) {
        return ExecuteNote(pos + 5);
    } else if (strncmp(pos, "chord(", 6) == 0) {
        return ExecuteChord(pos + 6);
    } else if (strncmp(pos, "arpeggio(", 9) == 0) {
        return ExecuteArpeggio(pos + 9);
    } else if (strncmp(pos, "progression(", 12) == 0) {
        return ExecuteProgression(pos + 12);
    }

    m_error.SetFormatted(256, "Unknown arranger call: %.50s", pos);
    return false;
}

// ============================================================================
// Parameter Parsing
// ============================================================================
bool Interpreter::ParseParams(const char* params, ArrangerParams& out) {
    // Simple parameter parser for key=value pairs
    std::string paramStr(params);

    // Remove trailing )
    size_t endPos = paramStr.find(')');
    if (endPos != std::string::npos) {
        paramStr = paramStr.substr(0, endPos);
    }

    // Parse key=value pairs
    size_t pos = 0;
    while (pos < paramStr.length()) {
        // Skip whitespace and commas
        while (pos < paramStr.length() && (paramStr[pos] == ' ' || paramStr[pos] == ',')) pos++;
        if (pos >= paramStr.length()) break;

        // Find key
        size_t eqPos = paramStr.find('=', pos);
        if (eqPos == std::string::npos) break;

        std::string key = paramStr.substr(pos, eqPos - pos);
        pos = eqPos + 1;

        // Find value
        std::string value;
        if (pos < paramStr.length() && paramStr[pos] == '"') {
            // Quoted string
            pos++;
            size_t endQuote = paramStr.find('"', pos);
            if (endQuote != std::string::npos) {
                value = paramStr.substr(pos, endQuote - pos);
                pos = endQuote + 1;
            }
        } else if (pos < paramStr.length() && paramStr[pos] == '[') {
            // Array (for progressions)
            size_t endBracket = paramStr.find(']', pos);
            if (endBracket != std::string::npos) {
                value = paramStr.substr(pos + 1, endBracket - pos - 1);
                pos = endBracket + 1;

                // Parse array elements
                if (key == "chords") {
                    size_t arrPos = 0;
                    while (arrPos < value.length()) {
                        while (arrPos < value.length() && (value[arrPos] == ' ' || value[arrPos] == ',')) arrPos++;
                        size_t end = value.find_first_of(", ]", arrPos);
                        if (end == std::string::npos) end = value.length();
                        if (end > arrPos) {
                            out.chords.push_back(value.substr(arrPos, end - arrPos));
                        }
                        arrPos = end + 1;
                    }
                }
            }
        } else {
            // Unquoted value
            size_t end = paramStr.find_first_of(", )", pos);
            if (end == std::string::npos) end = paramStr.length();
            value = paramStr.substr(pos, end - pos);
            pos = end;
        }

        // Store value
        if (key == "symbol" || key == "chord") out.symbol = value;
        else if (key == "pitch") out.pitch = value;
        else if (key == "duration") out.duration = atof(value.c_str());
        else if (key == "length") out.length = atof(value.c_str());
        else if (key == "note_duration") out.noteDuration = atof(value.c_str());
        else if (key == "start") out.start = atof(value.c_str());
        else if (key == "velocity") out.velocity = atoi(value.c_str());
        else if (key == "octave") out.octave = atoi(value.c_str());
        else if (key == "direction") out.direction = value;
    }

    return true;
}

// ============================================================================
// Execute Note
// ============================================================================
bool Interpreter::ExecuteNote(const char* params) {
    ArrangerParams p;
    if (!ParseParams(params, p)) {
        m_error.Set("Failed to parse note parameters");
        return false;
    }

    if (p.pitch.empty()) {
        m_error.Set("note() requires pitch parameter");
        return false;
    }

    int pitch = NoteToPitch(p.pitch.c_str());
    if (pitch < 0) {
        m_error.SetFormatted(128, "Invalid pitch: %s", p.pitch.c_str());
        return false;
    }

    MediaItem* item = GetOrCreateTargetItem(p.duration);
    if (!item) return false;

    return CreateMIDINote(item, pitch, m_startBeat + p.start, p.duration, p.velocity);
}

// ============================================================================
// Execute Chord
// ============================================================================
bool Interpreter::ExecuteChord(const char* params) {
    ArrangerParams p;
    if (!ParseParams(params, p)) {
        m_error.Set("Failed to parse chord parameters");
        return false;
    }

    if (p.symbol.empty()) {
        m_error.Set("chord() requires symbol parameter");
        return false;
    }

    int notes[8];
    int noteCount = 0;
    if (!ChordToNotes(p.symbol.c_str(), notes, noteCount, p.octave)) {
        m_error.SetFormatted(128, "Unknown chord symbol: %s", p.symbol.c_str());
        return false;
    }

    MediaItem* item = GetOrCreateTargetItem(p.length);
    if (!item) return false;

    // Create all notes simultaneously
    for (int i = 0; i < noteCount; i++) {
        if (!CreateMIDINote(item, notes[i], m_startBeat + p.start, p.length, p.velocity)) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Execute Arpeggio
// ============================================================================
bool Interpreter::ExecuteArpeggio(const char* params) {
    ArrangerParams p;
    if (!ParseParams(params, p)) {
        m_error.Set("Failed to parse arpeggio parameters");
        return false;
    }

    if (p.symbol.empty()) {
        m_error.Set("arpeggio() requires symbol parameter");
        return false;
    }

    int notes[8];
    int noteCount = 0;
    if (!ChordToNotes(p.symbol.c_str(), notes, noteCount, p.octave)) {
        m_error.SetFormatted(128, "Unknown chord symbol: %s", p.symbol.c_str());
        return false;
    }

    MediaItem* item = GetOrCreateTargetItem(p.length);
    if (!item) return false;

    // Calculate how many notes fit in the length
    int numNotes = (int)(p.length / p.noteDuration);
    double currentBeat = m_startBeat + p.start;

    for (int i = 0; i < numNotes; i++) {
        int noteIndex;
        if (p.direction == "up") {
            noteIndex = i % noteCount;
        } else if (p.direction == "down") {
            noteIndex = (noteCount - 1) - (i % noteCount);
        } else { // updown
            int cycle = noteCount * 2 - 2;
            int pos = i % cycle;
            if (pos < noteCount) {
                noteIndex = pos;
            } else {
                noteIndex = cycle - pos;
            }
        }

        if (!CreateMIDINote(item, notes[noteIndex], currentBeat, p.noteDuration, p.velocity)) {
            return false;
        }
        currentBeat += p.noteDuration;
    }

    return true;
}

// ============================================================================
// Execute Progression
// ============================================================================
bool Interpreter::ExecuteProgression(const char* params) {
    ArrangerParams p;
    if (!ParseParams(params, p)) {
        m_error.Set("Failed to parse progression parameters");
        return false;
    }

    if (p.chords.empty()) {
        m_error.Set("progression() requires chords array");
        return false;
    }

    MediaItem* item = GetOrCreateTargetItem(p.length);
    if (!item) return false;

    double chordLength = p.length / p.chords.size();
    double currentBeat = m_startBeat + p.start;

    for (const auto& chordSymbol : p.chords) {
        int notes[8];
        int noteCount = 0;
        if (!ChordToNotes(chordSymbol.c_str(), notes, noteCount, p.octave)) {
            continue; // Skip unknown chords
        }

        for (int i = 0; i < noteCount; i++) {
            CreateMIDINote(item, notes[i], currentBeat, chordLength, p.velocity);
        }
        currentBeat += chordLength;
    }

    return true;
}

// ============================================================================
// Note Name to MIDI Pitch
// ============================================================================
int Interpreter::NoteToPitch(const char* noteName) {
    if (!noteName || !*noteName) return -1;

    // Parse note letter
    int basePitch;
    switch (toupper(noteName[0])) {
        case 'C': basePitch = 0; break;
        case 'D': basePitch = 2; break;
        case 'E': basePitch = 4; break;
        case 'F': basePitch = 5; break;
        case 'G': basePitch = 7; break;
        case 'A': basePitch = 9; break;
        case 'B': basePitch = 11; break;
        default: return -1;
    }

    int pos = 1;

    // Check for sharp/flat
    if (noteName[pos] == '#') {
        basePitch++;
        pos++;
    } else if (noteName[pos] == 'b') {
        basePitch--;
        pos++;
    }

    // Parse octave
    int octave = 4; // Default
    if (noteName[pos] == '-') {
        pos++;
        octave = -atoi(&noteName[pos]);
    } else if (noteName[pos] >= '0' && noteName[pos] <= '9') {
        octave = atoi(&noteName[pos]);
    }

    // MIDI note = (octave + 1) * 12 + basePitch
    return (octave + 1) * 12 + basePitch;
}

// ============================================================================
// Chord Symbol to Notes
// ============================================================================
bool Interpreter::ChordToNotes(const char* symbol, int* notes, int& noteCount, int octave) {
    if (!symbol || !*symbol) return false;

    // Parse root note
    int rootPitch;
    switch (toupper(symbol[0])) {
        case 'C': rootPitch = 0; break;
        case 'D': rootPitch = 2; break;
        case 'E': rootPitch = 4; break;
        case 'F': rootPitch = 5; break;
        case 'G': rootPitch = 7; break;
        case 'A': rootPitch = 9; break;
        case 'B': rootPitch = 11; break;
        default: return false;
    }

    int pos = 1;
    if (symbol[pos] == '#') {
        rootPitch++;
        pos++;
    } else if (symbol[pos] == 'b') {
        rootPitch--;
        pos++;
    }

    // Base root in the specified octave
    int root = (octave + 1) * 12 + rootPitch;

    // Default: major triad
    int third = 4;  // Major third
    int fifth = 7;  // Perfect fifth
    int seventh = -1;
    bool hasSeventh = false;

    // Parse quality
    const char* quality = &symbol[pos];
    if (strncmp(quality, "m", 1) == 0 && (quality[1] == '\0' || quality[1] == '7' || !isalpha(quality[1]))) {
        third = 3;  // Minor third
        pos++;
    } else if (strncmp(quality, "dim", 3) == 0) {
        third = 3;
        fifth = 6;  // Diminished fifth
        pos += 3;
    } else if (strncmp(quality, "aug", 3) == 0) {
        fifth = 8;  // Augmented fifth
        pos += 3;
    } else if (strncmp(quality, "sus2", 4) == 0) {
        third = 2;  // Suspended 2nd
        pos += 4;
    } else if (strncmp(quality, "sus4", 4) == 0) {
        third = 5;  // Suspended 4th
        pos += 4;
    }

    // Parse extensions
    quality = &symbol[pos];
    if (strncmp(quality, "maj7", 4) == 0) {
        seventh = 11;
        hasSeventh = true;
    } else if (strncmp(quality, "min7", 4) == 0 || strcmp(quality, "7") == 0) {
        seventh = 10;  // Minor seventh
        hasSeventh = true;
    } else if (strncmp(quality, "dim7", 4) == 0) {
        seventh = 9;
        hasSeventh = true;
    }

    // Build chord
    noteCount = 0;
    notes[noteCount++] = root;
    notes[noteCount++] = root + third;
    notes[noteCount++] = root + fifth;
    if (hasSeventh) {
        notes[noteCount++] = root + seventh;
    }

    return true;
}

// ============================================================================
// Get Selected Track
// ============================================================================
MediaTrack* Interpreter::GetSelectedTrack() {
    if (m_targetTrack) return m_targetTrack;

    if (!g_rec) return nullptr;

    int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
    MediaTrack* (*GetTrack)(void*, int) = (MediaTrack* (*)(void*, int))g_rec->GetFunc("GetTrack");
    int (*IsTrackSelected)(MediaTrack*) = (int (*)(MediaTrack*))g_rec->GetFunc("IsTrackSelected");

    if (!GetNumTracks || !GetTrack || !IsTrackSelected) return nullptr;

    int numTracks = GetNumTracks();
    for (int i = 0; i < numTracks; i++) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (track && IsTrackSelected(track)) {
            return track;
        }
    }

    // If no track selected, return first track
    if (numTracks > 0) {
        return GetTrack(nullptr, 0);
    }

    return nullptr;
}

// ============================================================================
// Get Project Tempo
// ============================================================================
double Interpreter::GetTempo() {
    if (!g_rec) return 120.0;

    double (*GetProjectTimeSignature)(void*) = nullptr;
    double (*Master_GetTempo)() = (double (*)())g_rec->GetFunc("Master_GetTempo");

    if (Master_GetTempo) {
        return Master_GetTempo();
    }
    return 120.0;
}

// ============================================================================
// Get or Create Target Item
// ============================================================================
MediaItem* Interpreter::GetOrCreateTargetItem(double lengthBeats) {
    MediaTrack* track = GetSelectedTrack();
    if (!track) {
        m_error.Set("No track selected");
        return nullptr;
    }

    if (!g_rec) return nullptr;

    // Get REAPER functions
    MediaItem* (*AddMediaItemToTrack)(MediaTrack*) =
        (MediaItem* (*)(MediaTrack*))g_rec->GetFunc("AddMediaItemToTrack");
    void* (*AddTakeToMediaItem)(MediaItem*) =
        (void* (*)(MediaItem*))g_rec->GetFunc("AddTakeToMediaItem");
    bool (*SetMediaItemInfo_Value)(MediaItem*, const char*, double) =
        (bool (*)(MediaItem*, const char*, double))g_rec->GetFunc("SetMediaItemInfo_Value");
    double (*TimeMap_beatsToTime)(void*, double) =
        (double (*)(void*, double))g_rec->GetFunc("TimeMap_beatsToTime");

    if (!AddMediaItemToTrack || !AddTakeToMediaItem || !SetMediaItemInfo_Value) {
        m_error.Set("REAPER API functions not available");
        return nullptr;
    }

    // Create item
    MediaItem* item = AddMediaItemToTrack(track);
    if (!item) {
        m_error.Set("Failed to create media item");
        return nullptr;
    }

    // Add empty take
    AddTakeToMediaItem(item);

    // Set position and length
    double startTime = TimeMap_beatsToTime ? TimeMap_beatsToTime(nullptr, m_startBeat) : m_startBeat / 2.0;
    double lengthTime = TimeMap_beatsToTime ? TimeMap_beatsToTime(nullptr, lengthBeats) - TimeMap_beatsToTime(nullptr, 0) : lengthBeats / 2.0;

    SetMediaItemInfo_Value(item, "D_POSITION", startTime);
    SetMediaItemInfo_Value(item, "D_LENGTH", lengthTime);

    return item;
}

// ============================================================================
// Create MIDI Note
// ============================================================================
bool Interpreter::CreateMIDINote(MediaItem* item, int pitch, double startBeat, double duration, int velocity) {
    if (!item || !g_rec) return false;

    // Get take
    void* (*GetActiveTake)(MediaItem*) = (void* (*)(MediaItem*))g_rec->GetFunc("GetActiveTake");
    if (!GetActiveTake) return false;

    void* take = GetActiveTake(item);
    if (!take) return false;

    // Insert MIDI note
    bool (*MIDI_InsertNote)(void*, bool, bool, double, double, int, int, int, const bool*) =
        (bool (*)(void*, bool, bool, double, double, int, int, int, const bool*))g_rec->GetFunc("MIDI_InsertNote");

    if (!MIDI_InsertNote) {
        m_error.Set("MIDI_InsertNote not available");
        return false;
    }

    // Convert beats to PPQ (assuming 960 PPQ per beat)
    double ppqStart = startBeat * 960.0;
    double ppqEnd = (startBeat + duration) * 960.0;

    // channel 0, selected = false, muted = false
    bool success = MIDI_InsertNote(take, false, false, ppqStart, ppqEnd, 0, pitch, velocity, nullptr);

    if (!success) {
        m_error.SetFormatted(128, "Failed to insert MIDI note pitch=%d", pitch);
        return false;
    }

    // Log success
    if (g_rec) {
        void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
        if (ShowConsoleMsg) {
            char msg[128];
            snprintf(msg, sizeof(msg), "MAGDA Arranger: Created note pitch=%d beat=%.2f dur=%.2f\n",
                     pitch, startBeat, duration);
            ShowConsoleMsg(msg);
        }
    }

    return true;
}

} // namespace MagdaArranger
