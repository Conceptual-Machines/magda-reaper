#ifndef MAGDA_ARRANGER_INTERPRETER_H
#define MAGDA_ARRANGER_INTERPRETER_H

#include "../WDL/WDL/wdlstring.h"

// Forward declarations
class MediaTrack;
class MediaItem;

namespace MagdaArranger {

// ============================================================================
// Arranger DSL Interpreter
// ============================================================================
// Executes Arranger DSL (note, chord, arpeggio, progression)
// and creates MIDI notes on the selected track

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // Execute Arranger DSL code
    // Returns true on success
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

    // MIDI note creation
    bool CreateMIDINote(MediaItem* item, int pitch, double startBeat, double duration, int velocity);

    // Chord symbol to notes
    bool ChordToNotes(const char* symbol, int* notes, int& noteCount, int octave = 3);

    // Note name to MIDI pitch
    int NoteToPitch(const char* noteName);

    // Get or create target item
    MediaItem* GetOrCreateTargetItem(double lengthBeats);

    // Helper to get selected track
    MediaTrack* GetSelectedTrack();

    // Get project tempo
    double GetTempo();

    WDL_FastString m_error;
    MediaTrack* m_targetTrack;
    double m_startBeat;
};

} // namespace MagdaArranger

#endif // MAGDA_ARRANGER_INTERPRETER_H
