#ifndef MAGDA_ARRANGER_GRAMMAR_H
#define MAGDA_ARRANGER_GRAMMAR_H

// ============================================================================
// Arranger DSL Grammar (Lark format)
// ============================================================================
// Chord symbol-based musical composition for melodic/harmonic content
// Supports: note(), arpeggio(), chord(), progression()

static const char *ARRANGER_DSL_GRAMMAR = R"GRAMMAR(
// Arranger DSL Grammar - Chord symbol-based musical composition
// SIMPLE SYNTAX ONLY - one call per statement:
//   note(pitch=E1, duration=4) - single note
//   arpeggio(symbol=Em, note_duration=0.25) - sequential notes
//   chord(symbol=C, length=4) - simultaneous notes
//   progression(chords=[C, Am, F, G], length=16) - chord sequence

// ---------- Start rule ----------
start: statement

// ---------- Statements - ONE call only, no chaining ----------
statement: arpeggio_call
         | chord_call
         | progression_call
         | note_call

// ---------- Single Note ----------
note_call: "note" "(" note_params ")"

note_params: note_named_params

note_named_params: note_named_param ("," SP note_named_param)*
note_named_param: "pitch" "=" NOTE_NAME
               | "duration" "=" NUMBER
               | "velocity" "=" NUMBER
               | "start" "=" NUMBER

NOTE_NAME: /[A-G][#b]?-?[0-9]/

// ---------- Arpeggio: SEQUENTIAL notes ----------
arpeggio_call: "arpeggio" "(" arpeggio_params ")"

arpeggio_params: arpeggio_named_params

arpeggio_named_params: arpeggio_named_param ("," SP arpeggio_named_param)*
arpeggio_named_param: "symbol" "=" chord_symbol
                    | "chord" "=" chord_symbol
                    | "length" "=" NUMBER
                    | "start" "=" NUMBER
                    | "duration" "=" NUMBER
                    | "note_duration" "=" NUMBER
                    | "rhythm" "=" STRING
                    | "repeat" "=" NUMBER
                    | "velocity" "=" NUMBER
                    | "octave" "=" NUMBER
                    | "direction" "=" ("up" | "down" | "updown")

// ---------- Chord: SIMULTANEOUS notes ----------
chord_call: "chord" "(" chord_params ")"

chord_params: chord_named_params

chord_named_params: chord_named_param ("," SP chord_named_param)*
chord_named_param: "symbol" "=" chord_symbol
                 | "chord" "=" chord_symbol
                 | "length" "=" NUMBER
                 | "start" "=" NUMBER
                 | "duration" "=" NUMBER
                 | "rhythm" "=" STRING
                 | "repeat" "=" NUMBER
                 | "velocity" "=" NUMBER
                 | "inversion" "=" NUMBER

// ---------- Progression: sequence of chords ----------
progression_call: "progression" "(" progression_params ")"

progression_params: progression_named_params

progression_named_params: progression_named_param ("," SP progression_named_param)*
progression_named_param: "chords" "=" chords_array
                       | "length" "=" NUMBER
                       | "start" "=" NUMBER
                       | "repeat" "=" NUMBER

chords_array: "[" (chord_symbol ("," SP chord_symbol)*)? "]"

// ---------- Chord symbol (Em, C, Am7, Cmaj7, etc.) ----------
chord_symbol: CHORD_ROOT CHORD_QUALITY? CHORD_EXTENSION? CHORD_BASS?
CHORD_ROOT: /[A-G][#b]?/
CHORD_QUALITY: "m" | "dim" | "aug" | "sus2" | "sus4"
CHORD_EXTENSION: /[0-9]+/ | "maj7" | "min7" | "dim7" | "aug7" | "add9" | "add11" | "add13"
CHORD_BASS: "/" CHORD_ROOT

// ---------- Terminals ----------
SP: " "+
STRING: /"[^"]*"/
NUMBER: /-?\d+(\.\d+)?/
)GRAMMAR";

static const char *ARRANGER_TOOL_DESCRIPTION = R"DESC(
Generate ONE musical call. Choose exactly ONE:

1. NOTE (single sustained note): note(pitch="E1", duration=4)
   - pitch: Note name like E1, C4, F#3, Bb2 (octave 4 = middle C)
   - duration: Length in beats (1=quarter, 4=whole note/1 bar)
   - Use for 'sustained E1', 'add note C4', 'bass note', etc.

2. ARPEGGIO (sequential notes): arpeggio(symbol=Em, note_duration=0.25, length=8)
   - symbol: Chord symbol (Em, C, Am7, etc.)
   - note_duration: 0.25=16th, 0.5=8th, 1=quarter note
   - length: total beats (1 bar=4 beats, 2 bars=8 beats)

3. CHORD (simultaneous notes): chord(symbol=C, length=4)
   - symbol: Chord symbol
   - length: Duration in beats

4. PROGRESSION (chord sequence): progression(chords=[C, Am, F, G], length=16)
   - chords: Array of chord symbols
   - length: Total duration in beats

**CRITICAL: Output ONLY the DSL call. No explanations.**
)DESC";

#endif // MAGDA_ARRANGER_GRAMMAR_H
