#ifndef MAGDA_DRUMMER_GRAMMAR_H
#define MAGDA_DRUMMER_GRAMMAR_H

// ============================================================================
// Drummer DSL Grammar (Lark format)
// ============================================================================
// Grid-based drum pattern notation
// Grid: Each character = 1 subdivision (default 16th note)
//   "x" = hit (velocity 100), "X" = accent (velocity 127)
//   "-" = rest, "o" = ghost note (velocity 60)

static const char* DRUMMER_DSL_GRAMMAR = R"GRAMMAR(
// Drummer DSL Grammar - Grid-based drum pattern notation
// SYNTAX:
//   pattern(drum=kick, grid="x---x---x---x---")
//   pattern(drum=snare, grid="----x-------x---", velocity=100)
//
// GRID NOTATION (each char = 1 16th note):
//   "x" = hit (velocity 100)
//   "X" = accent (velocity 127)
//   "o" = ghost note (velocity 60)
//   "-" = rest
//
// DRUMS: kick, snare, hat, hat_open, tom_high, tom_mid, tom_low, crash, ride

// ---------- Start rule ----------
start: pattern_call (";" pattern_call)*

// ---------- Pattern ----------
pattern_call: "pattern" "(" pattern_params ")"

pattern_params: pattern_named_params

pattern_named_params: pattern_named_param ("," SP pattern_named_param)*
pattern_named_param: "drum" "=" DRUM_NAME
                   | "grid" "=" STRING
                   | "velocity" "=" NUMBER

// ---------- Drum names ----------
DRUM_NAME: "kick" | "snare" | "snare_rim" | "snare_xstick"
         | "hat" | "hat_open" | "hat_pedal"
         | "tom_high" | "tom_mid" | "tom_low"
         | "crash" | "ride" | "ride_bell" | "china" | "splash"
         | "cowbell" | "tambourine" | "clap" | "snap" | "shaker"

// ---------- Terminals ----------
SP: " "+
STRING: /"[^"]*"/
NUMBER: /-?\d+(\.\d+)?/
)GRAMMAR";

static const char* DRUMMER_TOOL_DESCRIPTION = R"DESC(
Generate drum patterns using grid notation.

SYNTAX: pattern(drum=<drum_name>, grid="<pattern>")
Multiple patterns separated by semicolon.

GRID NOTATION (16 chars = 1 bar of 16th notes):
- "x" = hit (velocity 100)
- "X" = accent (velocity 127)
- "o" = ghost note (velocity 60)
- "-" = rest

AVAILABLE DRUMS:
- kick, snare, snare_rim, snare_xstick
- hat, hat_open, hat_pedal
- tom_high, tom_mid, tom_low
- crash, ride, ride_bell, china, splash
- cowbell, tambourine, clap, snap, shaker

EXAMPLES:
- Basic rock beat:
  pattern(drum=kick, grid="x---x---x---x---");
  pattern(drum=snare, grid="----x-------x---");
  pattern(drum=hat, grid="x-x-x-x-x-x-x-x-")

- Breakbeat:
  pattern(drum=kick, grid="x--x--x---x-x---");
  pattern(drum=snare, grid="----x--x-x--x---")

**CRITICAL: Output ONLY the DSL calls. No explanations.**
)DESC";

#endif // MAGDA_DRUMMER_GRAMMAR_H
