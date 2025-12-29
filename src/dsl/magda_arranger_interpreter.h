#ifndef MAGDA_ARRANGER_INTERPRETER_H
#define MAGDA_ARRANGER_INTERPRETER_H

#include "../WDL/WDL/wdlstring.h"
#include <vector>

// Forward declarations
class MediaTrack;
class MediaItem;

namespace MagdaArranger {

// Simple note data structure
struct NoteData {
    int pitch;
    double start;   // in beats
    double length;  // in beats
    int velocity;
};

// ============================================================================
// Arranger DSL Interpreter
// ============================================================================
// Executes Arranger DSL (note, chord, arpeggio, progression)
// and creates MIDI notes using the existing AddMIDI function

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // Execute Arranger DSL code
    bool Execute(const char* dsl_code);

    // Get error message
    const char* GetError() const { return m_error.Get(); }

    // Set target track (uses selected track if not set)
    void SetTargetTrack(MediaTrack* track) { m_targetTrack = track; }

    // Set start position in beats (default 0)
    void SetStartBeat(double beat) { m_startBeat = beat; }

private:
    // Parse and execute individual calls
    bool ExecuteNote(const char* params);
    bool ExecuteChord(const char* params);
    bool ExecuteArpeggio(const char* params);
    bool ExecuteProgression(const char* params);

    // Parameter parsing
    bool ParseParams(const char* params, struct ArrangerParams& out);

    // Build JSON and call AddMIDI
    bool AddNotesToTrack(int trackIndex, const std::vector<NoteData>& notes, const char* name);

    // Get selected track index
    int GetSelectedTrackIndex();

    // Chord symbol to notes
    bool ChordToNotes(const char* symbol, int* notes, int& noteCount, int octave = 3);

    // Note name to MIDI pitch
    int NoteToPitch(const char* noteName);

    // Legacy stubs (not used, kept for interface compatibility)
    MediaTrack* GetSelectedTrack();
    double GetTempo();
    MediaItem* GetOrCreateTargetItem(double lengthBeats);
    bool CreateMIDINote(MediaItem* item, int pitch, double startBeat, double duration, int velocity);

    WDL_FastString m_error;
    MediaTrack* m_targetTrack;
    double m_startBeat;
};

} // namespace MagdaArranger

#endif // MAGDA_ARRANGER_INTERPRETER_H
