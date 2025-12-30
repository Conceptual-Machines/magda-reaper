#ifndef MAGDA_DSL_GRAMMAR_H
#define MAGDA_DSL_GRAMMAR_H

// ============================================================================
// MAGDA DSL Grammar (Lark format)
// ============================================================================
// This grammar is sent to OpenAI for CFG-constrained output.
// The LLM generates DSL code that matches this grammar exactly.
// The DSL interpreter then executes it directly against REAPER API.
//
// Grammar features:
// - track() - create new track or reference existing
// - Method chaining: .new_clip(), .set_track(), .add_fx(), .delete()
// - Filter operations: filter(tracks, track.name == "X")
// - Functional methods: .map(), .for_each()
//
// Examples:
//   track(instrument="Serum").new_clip(bar=3, length_bars=4)
//   track(id=1).set_track(mute=true)
//   filter(tracks, track.name == "Bass").delete()
// ============================================================================

static const char *MAGDA_DSL_GRAMMAR = R"GRAMMAR(
// MAGDA DSL Grammar - Lark format
// Functional DSL for REAPER operations

start: statement+

statement: track_statement
         | filter_statement
         | note_statement
         | chord_statement
         | arpeggio_statement
         | progression_statement
         | pattern_statement

// Track statements
track_statement: "track" "(" params? ")" chain?

// Filter statements (for bulk operations)
filter_statement: "filter" "(" "tracks" "," condition ")" chain?

// Musical content statements (added to most recently created track)
note_statement: "note" "(" params ")"
chord_statement: "chord" "(" params ")"
arpeggio_statement: "arpeggio" "(" params ")"
progression_statement: "progression" "(" params ")"
pattern_statement: "pattern" "(" params ")"

condition: "track" "." IDENTIFIER "==" value

// Method chain
chain: method+

method: "." method_call

method_call: "new_clip" "(" params? ")"
           | "set_track" "(" params? ")"
           | "add_fx" "(" params? ")"
           | "addAutomation" "(" params? ")"
           | "add_automation" "(" params? ")"
           | "delete" "(" ")"
           | "delete_clip" "(" params? ")"
           | "map" "(" func_ref ")"
           | "for_each" "(" func_ref ")"

// Function reference for map/for_each
func_ref: "@" IDENTIFIER

// Parameters
params: param ("," param)*

param: IDENTIFIER "=" value

value: STRING
     | NUMBER
     | BOOLEAN
     | IDENTIFIER
     | array

// Array for progression chords
array: "[" array_items? "]"
array_items: IDENTIFIER ("," IDENTIFIER)*

// Terminals
STRING: "\"" /[^"]*/ "\""
NUMBER: /-?[0-9]+(\.[0-9]+)?/
BOOLEAN: "true" | "false" | "True" | "False"
IDENTIFIER: /[a-zA-Z_#][a-zA-Z0-9_#]*/

// Whitespace and comments
%import common.WS
%ignore WS
COMMENT: "//" /[^\n]/*
%ignore COMMENT
)GRAMMAR";

// ============================================================================
// Tool description for OpenAI CFG
// ============================================================================
// This description helps the LLM understand when and how to use the DSL.

static const char *MAGDA_DSL_TOOL_DESCRIPTION = R"DESC(
**YOU MUST USE THIS TOOL TO GENERATE YOUR RESPONSE. DO NOT GENERATE TEXT OUTPUT DIRECTLY.**

Executes REAPER operations using the MAGDA DSL. Generate functional script code.
Each command goes on a separate line. Track operations execute FIRST, then musical content is added.

TRACK OPERATIONS:
- track() - Create new track
- track(name="Bass") - Create track with name
- track(instrument="Serum") - Create track with instrument (use @plugin_name format)
- track(id=1) - Reference existing track (1-based index)
- track(selected=true) - Reference selected track

METHOD CHAINING:
- .new_clip(bar=3, length_bars=4) - Create MIDI clip at bar
- .set_track(name="X", volume_db=-3, pan=0.5, mute=true, solo=true, selected=true)
- .add_fx(fxname="ReaEQ") - Add effect to track
- .delete() - Delete track

MUSICAL CONTENT (automatically added to most recently created track):
- note(pitch="E1", duration=4) - Single sustained note (duration in beats, 4=whole note)
- note(pitch="C4", duration=2, velocity=100) - Note with velocity (0-127)
- chord(symbol=Em, length=4) - Chord (Em, Am7, Cmaj7, etc.)
- arpeggio(symbol=Am, note_duration=0.25, length=4) - Arpeggio pattern
- progression(chords=[C, Am, F, G], length=16) - Chord progression
- pattern(drum=kick, grid="x---x---x---x---") - Drum pattern (x=hit, -=rest)

PITCH NOTATION: Note name + octave (E1=bass, C4=middle C, A4=440Hz)
Examples: E1, F#2, Bb3, C4, G#5

FILTER OPERATIONS (bulk):
- filter(tracks, track.name == "X").delete() - Delete all tracks named X
- filter(tracks, track.name == "X").set_track(mute=true) - Mute all tracks named X

EXAMPLES:
- "create track with Serum" → track(instrument="@serum")
- "add track with bass note E1" →
  track(name="Bass")
  note(pitch="E1", duration=4)
- "create synth track with C minor chord" →
  track(instrument="@serum", name="Synth")
  chord(symbol=Cm, length=4)
- "add sustained E1 for 8 beats" → note(pitch="E1", duration=8)
- "delete track 1" → track(id=1).delete()
- "mute all tracks named Drums" → filter(tracks, track.name == "Drums").set_track(mute=true)
- "add reverb to track 2" → track(id=2).add_fx(fxname="ReaVerbate")

**CRITICAL: Always generate DSL code. Never generate plain text responses.**
)DESC";

#endif // MAGDA_DSL_GRAMMAR_H
