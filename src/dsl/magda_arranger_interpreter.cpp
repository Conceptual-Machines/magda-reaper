#include "magda_arranger_interpreter.h"
#include "../WDL/WDL/jsonparse.h"
#include "magda_actions.h"
#include "magda_dsl_context.h"
#include "reaper_plugin.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern reaper_plugin_info_t *g_rec;

namespace MagdaArranger {

// ============================================================================
// Arranger Parameters
// ============================================================================
struct ArrangerParams {
  std::string symbol;         // Chord symbol (Em, C, Am7)
  std::string pitch;          // Note pitch for single notes
  double duration = 4.0;      // Duration in beats
  double length = 4.0;        // Total length in beats
  double noteDuration = 0.25; // Note duration for arpeggios
  double start = 0.0;         // Start position in beats
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

// Helper to log
static void Log(const char *fmt, ...) {
  if (!g_rec)
    return;
  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (!ShowConsoleMsg)
    return;

  char msg[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  ShowConsoleMsg(msg);
}

bool Interpreter::Execute(const char *dsl_code) {
  if (!dsl_code || !*dsl_code) {
    m_error.Set("Empty DSL code");
    return false;
  }

  Log("MAGDA Arranger: Executing: %s\n", dsl_code);

  // Find the call type
  const char *pos = dsl_code;
  while (*pos && (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t'))
    pos++;

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
bool Interpreter::ParseParams(const char *params, ArrangerParams &out) {
  std::string paramStr(params);

  // Remove trailing )
  size_t endPos = paramStr.find(')');
  if (endPos != std::string::npos) {
    paramStr = paramStr.substr(0, endPos);
  }

  // Parse key=value pairs
  size_t pos = 0;
  while (pos < paramStr.length()) {
    while (pos < paramStr.length() && (paramStr[pos] == ' ' || paramStr[pos] == ','))
      pos++;
    if (pos >= paramStr.length())
      break;

    size_t eqPos = paramStr.find('=', pos);
    if (eqPos == std::string::npos)
      break;

    std::string key = paramStr.substr(pos, eqPos - pos);
    pos = eqPos + 1;

    std::string value;
    if (pos < paramStr.length() && paramStr[pos] == '"') {
      pos++;
      size_t endQuote = paramStr.find('"', pos);
      if (endQuote != std::string::npos) {
        value = paramStr.substr(pos, endQuote - pos);
        pos = endQuote + 1;
      }
    } else if (pos < paramStr.length() && paramStr[pos] == '[') {
      size_t endBracket = paramStr.find(']', pos);
      if (endBracket != std::string::npos) {
        value = paramStr.substr(pos + 1, endBracket - pos - 1);
        pos = endBracket + 1;

        if (key == "chords") {
          size_t arrPos = 0;
          while (arrPos < value.length()) {
            while (arrPos < value.length() && (value[arrPos] == ' ' || value[arrPos] == ','))
              arrPos++;
            size_t end = value.find_first_of(", ]", arrPos);
            if (end == std::string::npos)
              end = value.length();
            if (end > arrPos) {
              out.chords.push_back(value.substr(arrPos, end - arrPos));
            }
            arrPos = end + 1;
          }
        }
      }
    } else {
      size_t end = paramStr.find_first_of(", )", pos);
      if (end == std::string::npos)
        end = paramStr.length();
      value = paramStr.substr(pos, end - pos);
      pos = end;
    }

    if (key == "symbol" || key == "chord")
      out.symbol = value;
    else if (key == "pitch")
      out.pitch = value;
    else if (key == "duration")
      out.duration = atof(value.c_str());
    else if (key == "length")
      out.length = atof(value.c_str());
    else if (key == "note_duration")
      out.noteDuration = atof(value.c_str());
    else if (key == "start")
      out.start = atof(value.c_str());
    else if (key == "velocity")
      out.velocity = atoi(value.c_str());
    else if (key == "octave")
      out.octave = atoi(value.c_str());
    else if (key == "direction")
      out.direction = value;
  }

  return true;
}

// ============================================================================
// Note Name to MIDI Pitch
// ============================================================================
int Interpreter::NoteToPitch(const char *noteName) {
  if (!noteName || !*noteName)
    return -1;

  int basePitch;
  switch (toupper(noteName[0])) {
  case 'C':
    basePitch = 0;
    break;
  case 'D':
    basePitch = 2;
    break;
  case 'E':
    basePitch = 4;
    break;
  case 'F':
    basePitch = 5;
    break;
  case 'G':
    basePitch = 7;
    break;
  case 'A':
    basePitch = 9;
    break;
  case 'B':
    basePitch = 11;
    break;
  default:
    return -1;
  }

  int pos = 1;
  if (noteName[pos] == '#') {
    basePitch++;
    pos++;
  } else if (noteName[pos] == 'b') {
    basePitch--;
    pos++;
  }

  int octave = 4;
  if (noteName[pos] == '-') {
    pos++;
    octave = -atoi(&noteName[pos]);
  } else if (noteName[pos] >= '0' && noteName[pos] <= '9') {
    octave = atoi(&noteName[pos]);
  }

  return (octave + 1) * 12 + basePitch;
}

// ============================================================================
// Chord Symbol to Notes
// ============================================================================
bool Interpreter::ChordToNotes(const char *symbol, int *notes, int &noteCount, int octave) {
  if (!symbol || !*symbol)
    return false;

  int rootPitch;
  switch (toupper(symbol[0])) {
  case 'C':
    rootPitch = 0;
    break;
  case 'D':
    rootPitch = 2;
    break;
  case 'E':
    rootPitch = 4;
    break;
  case 'F':
    rootPitch = 5;
    break;
  case 'G':
    rootPitch = 7;
    break;
  case 'A':
    rootPitch = 9;
    break;
  case 'B':
    rootPitch = 11;
    break;
  default:
    return false;
  }

  int pos = 1;
  if (symbol[pos] == '#') {
    rootPitch++;
    pos++;
  } else if (symbol[pos] == 'b') {
    rootPitch--;
    pos++;
  }

  int root = (octave + 1) * 12 + rootPitch;
  int third = 4, fifth = 7, seventh = -1;
  bool hasSeventh = false;

  const char *quality = &symbol[pos];
  if (strncmp(quality, "m", 1) == 0 &&
      (quality[1] == '\0' || quality[1] == '7' || !isalpha(quality[1]))) {
    third = 3;
    pos++;
  } else if (strncmp(quality, "dim", 3) == 0) {
    third = 3;
    fifth = 6;
    pos += 3;
  } else if (strncmp(quality, "aug", 3) == 0) {
    fifth = 8;
    pos += 3;
  } else if (strncmp(quality, "sus2", 4) == 0) {
    third = 2;
    pos += 4;
  } else if (strncmp(quality, "sus4", 4) == 0) {
    third = 5;
    pos += 4;
  }

  quality = &symbol[pos];
  if (strncmp(quality, "maj7", 4) == 0) {
    seventh = 11;
    hasSeventh = true;
  } else if (strncmp(quality, "min7", 4) == 0 || strcmp(quality, "7") == 0) {
    seventh = 10;
    hasSeventh = true;
  } else if (strncmp(quality, "dim7", 4) == 0) {
    seventh = 9;
    hasSeventh = true;
  }

  noteCount = 0;
  notes[noteCount++] = root;
  notes[noteCount++] = root + third;
  notes[noteCount++] = root + fifth;
  if (hasSeventh)
    notes[noteCount++] = root + seventh;

  return true;
}

// ============================================================================
// Build JSON notes array and call AddMIDI
// ============================================================================
bool Interpreter::AddNotesToTrack(int trackIndex, const std::vector<NoteData> &notes,
                                  const char *name) {
  if (notes.empty()) {
    m_error.Set("No notes to add");
    return false;
  }

  // Build JSON: [{"pitch":60,"start":0,"length":1,"velocity":100}, ...]
  WDL_FastString json;
  json.Set("[");
  for (size_t i = 0; i < notes.size(); i++) {
    if (i > 0)
      json.Append(",");
    json.AppendFormatted(128, "{\"pitch\":%d,\"start\":%.4f,\"length\":%.4f,\"velocity\":%d}",
                         notes[i].pitch, notes[i].start, notes[i].length, notes[i].velocity);
  }
  json.Append("]");

  Log("MAGDA Arranger: Adding %d notes to track %d: %s\n", (int)notes.size(), trackIndex,
      json.Get());

  // Parse JSON
  wdl_json_parser parser;
  wdl_json_element *root = parser.parse(json.Get(), json.GetLength());
  if (!root || !root->is_array()) {
    m_error.Set("Failed to build notes JSON");
    return false;
  }

  // Call AddMIDI
  WDL_FastString errorMsg;
  bool success = MagdaActions::AddMIDI(trackIndex, root, name, errorMsg);

  if (!success) {
    m_error.SetFormatted(512, "AddMIDI failed: %s", errorMsg.Get());
  }

  return success;
}

// Get target track index using smart context resolution
int Interpreter::GetSelectedTrackIndex() {
  return MagdaDSLContext::Get().ResolveTargetTrack(nullptr);
}

// ============================================================================
// Execute Note
// ============================================================================
bool Interpreter::ExecuteNote(const char *params) {
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

  std::vector<NoteData> notes;
  notes.push_back({pitch, m_startBeat + p.start, p.duration, p.velocity});

  return AddNotesToTrack(GetSelectedTrackIndex(), notes, "Note");
}

// ============================================================================
// Execute Chord
// ============================================================================
bool Interpreter::ExecuteChord(const char *params) {
  ArrangerParams p;
  if (!ParseParams(params, p)) {
    m_error.Set("Failed to parse chord parameters");
    return false;
  }

  if (p.symbol.empty()) {
    m_error.Set("chord() requires symbol parameter");
    return false;
  }

  int pitches[8];
  int noteCount = 0;
  if (!ChordToNotes(p.symbol.c_str(), pitches, noteCount, p.octave)) {
    m_error.SetFormatted(128, "Unknown chord symbol: %s", p.symbol.c_str());
    return false;
  }

  std::vector<NoteData> notes;
  for (int i = 0; i < noteCount; i++) {
    notes.push_back({pitches[i], m_startBeat + p.start, p.length, p.velocity});
  }

  return AddNotesToTrack(GetSelectedTrackIndex(), notes, p.symbol.c_str());
}

// ============================================================================
// Execute Arpeggio
// ============================================================================
bool Interpreter::ExecuteArpeggio(const char *params) {
  ArrangerParams p;
  if (!ParseParams(params, p)) {
    m_error.Set("Failed to parse arpeggio parameters");
    return false;
  }

  if (p.symbol.empty()) {
    m_error.Set("arpeggio() requires symbol parameter");
    return false;
  }

  int pitches[8];
  int noteCount = 0;
  if (!ChordToNotes(p.symbol.c_str(), pitches, noteCount, p.octave)) {
    m_error.SetFormatted(128, "Unknown chord symbol: %s", p.symbol.c_str());
    return false;
  }

  Log("MAGDA Arranger: Chord %s = %d notes\n", p.symbol.c_str(), noteCount);

  int numNotes = (int)(p.length / p.noteDuration);
  std::vector<NoteData> notes;
  double currentBeat = m_startBeat + p.start;

  for (int i = 0; i < numNotes; i++) {
    int noteIndex;
    if (p.direction == "up") {
      noteIndex = i % noteCount;
    } else if (p.direction == "down") {
      noteIndex = (noteCount - 1) - (i % noteCount);
    } else {
      int cycle = noteCount * 2 - 2;
      if (cycle <= 0)
        cycle = 1;
      int pos = i % cycle;
      noteIndex = (pos < noteCount) ? pos : cycle - pos;
    }

    notes.push_back({pitches[noteIndex], currentBeat, p.noteDuration, p.velocity});
    currentBeat += p.noteDuration;
  }

  return AddNotesToTrack(GetSelectedTrackIndex(), notes, p.symbol.c_str());
}

// ============================================================================
// Execute Progression
// ============================================================================
bool Interpreter::ExecuteProgression(const char *params) {
  ArrangerParams p;
  if (!ParseParams(params, p)) {
    m_error.Set("Failed to parse progression parameters");
    return false;
  }

  if (p.chords.empty()) {
    m_error.Set("progression() requires chords array");
    return false;
  }

  double chordLength = p.length / p.chords.size();
  double currentBeat = m_startBeat + p.start;
  std::vector<NoteData> notes;

  for (const auto &chordSymbol : p.chords) {
    int pitches[8];
    int noteCount = 0;
    if (!ChordToNotes(chordSymbol.c_str(), pitches, noteCount, p.octave)) {
      continue;
    }

    for (int i = 0; i < noteCount; i++) {
      notes.push_back({pitches[i], currentBeat, chordLength, p.velocity});
    }
    currentBeat += chordLength;
  }

  return AddNotesToTrack(GetSelectedTrackIndex(), notes, "Progression");
}

// Stub implementations for interface compatibility
MediaTrack *Interpreter::GetSelectedTrack() {
  return nullptr;
}
double Interpreter::GetTempo() {
  return 120.0;
}
MediaItem *Interpreter::GetOrCreateTargetItem(double) {
  return nullptr;
}
bool Interpreter::CreateMIDINote(MediaItem *, int, double, double, int) {
  return false;
}

} // namespace MagdaArranger
