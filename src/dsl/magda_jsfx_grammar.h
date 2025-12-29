#ifndef MAGDA_JSFX_GRAMMAR_H
#define MAGDA_JSFX_GRAMMAR_H

// ============================================================================
// JSFX Grammar (Lark format)
// ============================================================================
// Validates raw JSFX/EEL2 output structure

static const char* JSFX_GRAMMAR = R"GRAMMAR(
// JSFX Direct Grammar - validates raw JSFX effect structure
// The LLM outputs actual JSFX code that REAPER can load directly

start: jsfx_effect

jsfx_effect: header_section slider_section? code_sections

// ========== Header Section ==========
header_section: desc_line tags_line? pin_lines? import_lines? option_lines? filename_lines?

desc_line: "desc:" REST_OF_LINE NEWLINE
tags_line: "tags:" REST_OF_LINE NEWLINE
pin_lines: pin_line+
pin_line: ("in_pin:" | "out_pin:") REST_OF_LINE NEWLINE
import_lines: import_line+
import_line: "import" REST_OF_LINE NEWLINE
option_lines: option_line+
option_line: "options:" REST_OF_LINE NEWLINE
filename_lines: filename_line+
filename_line: "filename:" NUMBER "," REST_OF_LINE NEWLINE

// ========== Slider Section ==========
slider_section: slider_line+
slider_line: "slider" NUMBER ":" SLIDER_DEF NEWLINE

SLIDER_DEF: /[^\n]+/

// ========== Code Sections ==========
code_sections: code_section*

code_section: init_section
            | slider_code_section
            | block_section
            | sample_section
            | serialize_section
            | gfx_section

init_section: "@init" NEWLINE eel2_code
slider_code_section: "@slider" NEWLINE eel2_code
block_section: "@block" NEWLINE eel2_code
sample_section: "@sample" NEWLINE eel2_code
serialize_section: "@serialize" NEWLINE eel2_code
gfx_section: "@gfx" GFX_SIZE? NEWLINE eel2_code

GFX_SIZE: /\s+\d+\s+\d+/

// ========== EEL2 Code Block ==========
eel2_code: EEL2_LINE*
EEL2_LINE: /[^@\n][^\n]*/ NEWLINE
         | NEWLINE

// ========== Terminals ==========
REST_OF_LINE: /[^\n]*/
NUMBER: /\d+/
NEWLINE: /\n/

COMMENT: /\/\/[^\n]*/
%ignore COMMENT
)GRAMMAR";

static const char* JSFX_TOOL_DESCRIPTION = R"DESC(
Generate a complete JSFX effect for REAPER.

OUTPUT FORMAT - Raw JSFX code:
desc:Effect Name
tags:category
in_pin:Left
in_pin:Right
out_pin:Left
out_pin:Right

slider1:param=default<min,max,step>Label

@init
// Initialize variables

@slider
// Update when slider changes

@sample
// Process each sample
spl0 = spl0 * gain;
spl1 = spl1 * gain;

REQUIREMENTS:
- Always include desc: line
- Define in_pin/out_pin for stereo
- Use meaningful slider names
- Initialize ALL variables in @init
- Handle edge cases

**CRITICAL: Output ONLY valid JSFX code. No explanations.**
)DESC";

// Comprehensive JSFX system prompt
static const char* JSFX_SYSTEM_PROMPT = R"PROMPT(
You are a JSFX expert. Generate complete, working REAPER JSFX effects.
Output raw JSFX code that can be saved directly as a .jsfx file.

JSFX FILE STRUCTURE:
desc:Effect Name
slider1:var=default<min,max,step>Label

@init
// Initialize (runs once)

@slider
// Runs when slider changes

@sample
// Process each sample
spl0 *= gain;
spl1 *= gain;

AUDIO VARIABLES: spl0-spl63, srate, samplesblock, num_ch
MATH: sin, cos, exp, log, sqrt, min, max, abs, floor, ceil
MEMORY: buf[i], freembuf(size), memset, memcpy

OUTPUT ONLY VALID JSFX CODE - NO EXPLANATIONS.
)PROMPT";

#endif // MAGDA_JSFX_GRAMMAR_H
