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

static const char* MAGDA_DSL_GRAMMAR = R"GRAMMAR(
// MAGDA DSL Grammar - Lark format
// Functional DSL for REAPER operations

start: statement+

statement: track_statement
         | filter_statement

// Track statements
track_statement: "track" "(" params? ")" chain?

// Filter statements (for bulk operations)
filter_statement: "filter" "(" "tracks" "," condition ")" chain?

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

// Terminals
STRING: "\"" /[^"]*/ "\""
NUMBER: /-?[0-9]+(\.[0-9]+)?/
BOOLEAN: "true" | "false" | "True" | "False"
IDENTIFIER: /[a-zA-Z_][a-zA-Z0-9_]*/

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

static const char* MAGDA_DSL_TOOL_DESCRIPTION = R"DESC(
**YOU MUST USE THIS TOOL TO GENERATE YOUR RESPONSE. DO NOT GENERATE TEXT OUTPUT DIRECTLY.**

Executes REAPER operations using the MAGDA DSL. Generate functional script code.

TRACK OPERATIONS:
- track() - Create new track
- track(name="Bass") - Create track with name
- track(instrument="Serum") - Create track with instrument
- track(id=1) - Reference existing track (1-based index)
- track(selected=true) - Reference selected track

METHOD CHAINING:
- .new_clip(bar=3, length_bars=4) - Create MIDI clip at bar
- .set_track(name="X", volume_db=-3, pan=0.5, mute=true, solo=true, selected=true)
- .add_fx(fxname="ReaEQ") - Add effect to track
- .add_automation(param="volume", curve="fade_in", start=0, end=4)
- .delete() - Delete track

FILTER OPERATIONS (bulk):
- filter(tracks, track.name == "X").delete() - Delete all tracks named X
- filter(tracks, track.name == "X").set_track(mute=true) - Mute all tracks named X

AUTOMATION CURVES:
- fade_in, fade_out - Linear fades
- ramp - Linear sweep from/to values
- sine, saw, square - Oscillator curves (use freq, amplitude params)
- exp_in, exp_out - Exponential curves

EXAMPLES:
- "create track with Serum" → track(instrument="Serum")
- "create 3 tracks" → track(); track(); track()
- "delete track 1" → track(id=1).delete()
- "mute all tracks named Drums" → filter(tracks, track.name == "Drums").set_track(mute=true)
- "add reverb to track 2" → track(id=2).add_fx(fxname="ReaVerbate")
- "create clip at bar 5" → track(selected=true).new_clip(bar=5, length_bars=4)

**CRITICAL: Always generate DSL code. Never generate plain text responses.**
)DESC";

#endif // MAGDA_DSL_GRAMMAR_H
