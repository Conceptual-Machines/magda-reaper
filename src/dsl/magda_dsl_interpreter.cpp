#include "magda_dsl_interpreter.h"
#include "magda_dsl_context.h"
#include "reaper_plugin.h"
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

extern reaper_plugin_info_t *g_rec;

namespace MagdaDSL {

// ============================================================================
// InterpreterContext
// ============================================================================

void InterpreterContext::SetErrorF(const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  error.Set(buf);
  has_error = true;
}

// ============================================================================
// Params Implementation
// ============================================================================

void Params::Set(const std::string &key, const std::string &value) {
  m_params[key] = value;
}

void Params::SetInt(const std::string &key, int value) {
  m_params[key] = std::to_string(value);
}

void Params::SetFloat(const std::string &key, double value) {
  m_params[key] = std::to_string(value);
}

void Params::SetBool(const std::string &key, bool value) {
  m_params[key] = value ? "true" : "false";
}

bool Params::Has(const std::string &key) const {
  return m_params.find(key) != m_params.end();
}

std::string Params::Get(const std::string &key, const std::string &def) const {
  auto it = m_params.find(key);
  return (it != m_params.end()) ? it->second : def;
}

int Params::GetInt(const std::string &key, int def) const {
  auto it = m_params.find(key);
  if (it == m_params.end())
    return def;
  return atoi(it->second.c_str());
}

double Params::GetFloat(const std::string &key, double def) const {
  auto it = m_params.find(key);
  if (it == m_params.end())
    return def;
  return atof(it->second.c_str());
}

bool Params::GetBool(const std::string &key, bool def) const {
  auto it = m_params.find(key);
  if (it == m_params.end())
    return def;
  return it->second == "true" || it->second == "True" || it->second == "1";
}

// ============================================================================
// Tokenizer Implementation
// ============================================================================

Tokenizer::Tokenizer(const char *input)
    : m_input(input), m_pos(input), m_line(1), m_col(1), m_has_peeked(false) {}

void Tokenizer::SkipWhitespace() {
  while (*m_pos) {
    if (*m_pos == ' ' || *m_pos == '\t' || *m_pos == '\r') {
      m_pos++;
      m_col++;
    } else if (*m_pos == '\n') {
      m_pos++;
      m_line++;
      m_col = 1;
    } else if (*m_pos == '/' && *(m_pos + 1) == '/') {
      // Skip line comment
      SkipComment();
    } else {
      break;
    }
  }
}

void Tokenizer::SkipComment() {
  while (*m_pos && *m_pos != '\n') {
    m_pos++;
  }
}

Token Tokenizer::ReadIdentifier() {
  int start_col = m_col;
  const char *start = m_pos;

  while (*m_pos && (isalnum(*m_pos) || *m_pos == '_')) {
    m_pos++;
    m_col++;
  }

  return Token(TokenType::IDENTIFIER, std::string(start, m_pos - start), m_line, start_col);
}

Token Tokenizer::ReadString() {
  int start_col = m_col;
  m_pos++; // Skip opening quote
  m_col++;

  std::string value;
  while (*m_pos && *m_pos != '"') {
    if (*m_pos == '\\' && *(m_pos + 1)) {
      m_pos++;
      m_col++;
      switch (*m_pos) {
      case 'n':
        value += '\n';
        break;
      case 't':
        value += '\t';
        break;
      case 'r':
        value += '\r';
        break;
      case '"':
        value += '"';
        break;
      case '\\':
        value += '\\';
        break;
      default:
        value += *m_pos;
        break;
      }
    } else {
      value += *m_pos;
    }
    m_pos++;
    m_col++;
  }

  if (*m_pos == '"') {
    m_pos++; // Skip closing quote
    m_col++;
  }

  return Token(TokenType::STRING, value, m_line, start_col);
}

Token Tokenizer::ReadNumber() {
  int start_col = m_col;
  const char *start = m_pos;

  // Handle negative numbers
  if (*m_pos == '-') {
    m_pos++;
    m_col++;
  }

  // Integer part
  while (*m_pos && isdigit(*m_pos)) {
    m_pos++;
    m_col++;
  }

  // Decimal part
  if (*m_pos == '.') {
    m_pos++;
    m_col++;
    while (*m_pos && isdigit(*m_pos)) {
      m_pos++;
      m_col++;
    }
  }

  return Token(TokenType::NUMBER, std::string(start, m_pos - start), m_line, start_col);
}

Token Tokenizer::Next() {
  if (m_has_peeked) {
    m_has_peeked = false;
    return m_peeked;
  }

  SkipWhitespace();

  if (!*m_pos) {
    return Token(TokenType::END_OF_INPUT, "", m_line, m_col);
  }

  int start_col = m_col;
  char c = *m_pos;

  // Single character tokens
  switch (c) {
  case '(':
    m_pos++;
    m_col++;
    return Token(TokenType::LPAREN, "(", m_line, start_col);
  case ')':
    m_pos++;
    m_col++;
    return Token(TokenType::RPAREN, ")", m_line, start_col);
  case '[':
    m_pos++;
    m_col++;
    return Token(TokenType::LBRACKET, "[", m_line, start_col);
  case ']':
    m_pos++;
    m_col++;
    return Token(TokenType::RBRACKET, "]", m_line, start_col);
  case '{':
    m_pos++;
    m_col++;
    return Token(TokenType::LBRACE, "{", m_line, start_col);
  case '}':
    m_pos++;
    m_col++;
    return Token(TokenType::RBRACE, "}", m_line, start_col);
  case '.':
    m_pos++;
    m_col++;
    return Token(TokenType::DOT, ".", m_line, start_col);
  case ',':
    m_pos++;
    m_col++;
    return Token(TokenType::COMMA, ",", m_line, start_col);
  case ';':
    m_pos++;
    m_col++;
    return Token(TokenType::SEMICOLON, ";", m_line, start_col);
  case '@':
    m_pos++;
    m_col++;
    return Token(TokenType::AT, "@", m_line, start_col);
  case '=':
    m_pos++;
    m_col++;
    if (*m_pos == '=') {
      m_pos++;
      m_col++;
      return Token(TokenType::EQUALS_EQUALS, "==", m_line, start_col);
    }
    return Token(TokenType::EQUALS, "=", m_line, start_col);
  }

  // String
  if (c == '"') {
    return ReadString();
  }

  // Number (including negative)
  if (isdigit(c) || (c == '-' && isdigit(*(m_pos + 1)))) {
    return ReadNumber();
  }

  // Identifier
  if (isalpha(c) || c == '_') {
    return ReadIdentifier();
  }

  // Unknown character
  m_error.SetFormatted(256, "Unexpected character '%c' at line %d, col %d", c, m_line, m_col);
  m_pos++;
  m_col++;
  return Token(TokenType::ERROR, std::string(1, c), m_line, start_col);
}

Token Tokenizer::Peek() {
  if (!m_has_peeked) {
    m_peeked = Next();
    m_has_peeked = true;
  }
  return m_peeked;
}

bool Tokenizer::HasMore() const {
  if (m_has_peeked) {
    return m_peeked.type != TokenType::END_OF_INPUT;
  }
  // Skip whitespace to check
  const char *p = m_pos;
  while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
    p++;
  return *p != '\0';
}

bool Tokenizer::Expect(TokenType type) {
  Token t = Next();
  return t.type == type;
}

bool Tokenizer::Expect(const char *identifier) {
  Token t = Next();
  return t.type == TokenType::IDENTIFIER && t.value == identifier;
}

// ============================================================================
// Interpreter Implementation
// ============================================================================

Interpreter::Interpreter() {}

Interpreter::~Interpreter() {}

void Interpreter::SetState(const std::map<std::string, std::string> &state) {
  // Could store state for track lookups if needed
  (void)state;
}

bool Interpreter::Execute(const char *dsl_code) {
  if (!dsl_code || !*dsl_code) {
    m_ctx.SetError("Empty DSL code");
    return false;
  }

  // Reset context
  m_ctx = InterpreterContext();

  // Log execution start
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA DSL: Executing: %.200s%s\n", dsl_code,
               strlen(dsl_code) > 200 ? "..." : "");
      ShowConsoleMsg(msg);
    }
  }

  Tokenizer tok(dsl_code);

  // Parse statements until end of input
  while (tok.HasMore()) {
    if (!ParseStatement(tok)) {
      return false;
    }

    // Optional semicolon between statements
    if (tok.Peek().Is(TokenType::SEMICOLON)) {
      tok.Next();
    }
  }

  // Log success
  if (g_rec) {
    void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA DSL: Execution complete\n");
    }
  }

  return true;
}

bool Interpreter::ParseStatement(Tokenizer &tok) {
  Token t = tok.Peek();

  if (t.Is("track")) {
    return ParseTrackStatement(tok);
  } else if (t.Is("filter")) {
    return ParseFilterStatement(tok);
  } else if (t.type == TokenType::END_OF_INPUT) {
    return true;
  } else {
    m_ctx.SetErrorF("Unexpected token '%s' at line %d", t.value.c_str(), t.line);
    return false;
  }
}

bool Interpreter::ParseTrackStatement(Tokenizer &tok) {
  tok.Next(); // consume 'track'

  if (!tok.Expect(TokenType::LPAREN)) {
    m_ctx.SetError("Expected '(' after 'track'");
    return false;
  }

  // Parse parameters
  Params params;
  if (!ParseParams(tok, params)) {
    return false;
  }

  if (!tok.Expect(TokenType::RPAREN)) {
    m_ctx.SetError("Expected ')' after track parameters");
    return false;
  }

  // Determine if this is track creation or track reference
  if (params.Has("id")) {
    // track(id=1) - reference existing track (1-based)
    int id = params.GetInt("id");
    m_ctx.current_track = GetTrackById(id);
    if (!m_ctx.current_track) {
      m_ctx.SetErrorF("Track %d not found", id);
      return false;
    }
    m_ctx.current_track_idx = id - 1;
  } else if (params.Has("selected") && params.GetBool("selected")) {
    // track(selected=true) - reference selected track
    m_ctx.current_track = GetSelectedTrack();
    if (!m_ctx.current_track) {
      m_ctx.SetError("No track is selected");
      return false;
    }
    // Get index
    double (*GetMediaTrackInfo_Value)(MediaTrack *, const char *) =
        (double (*)(MediaTrack *, const char *))g_rec->GetFunc("GetMediaTrackInfo_Value");
    if (GetMediaTrackInfo_Value) {
      m_ctx.current_track_idx =
          (int)GetMediaTrackInfo_Value(m_ctx.current_track, "IP_TRACKNUMBER") - 1;
    }
  } else {
    // track() or track(name="...", instrument="...") - create new track
    m_ctx.current_track = CreateTrack(params);
    if (!m_ctx.current_track) {
      return false;
    }
  }

  // Parse method chain
  return ParseMethodChain(tok);
}

bool Interpreter::ParseFilterStatement(Tokenizer &tok) {
  tok.Next(); // consume 'filter'

  if (!tok.Expect(TokenType::LPAREN)) {
    m_ctx.SetError("Expected '(' after 'filter'");
    return false;
  }

  // Expect: filter(tracks, track.name == "value")
  Token collection = tok.Next();
  if (!collection.Is("tracks")) {
    m_ctx.SetErrorF("Expected 'tracks' in filter, got '%s'", collection.value.c_str());
    return false;
  }

  if (!tok.Expect(TokenType::COMMA)) {
    m_ctx.SetError("Expected ',' after 'tracks' in filter");
    return false;
  }

  // Parse condition: track.field == "value"
  Token trackToken = tok.Next();
  if (!trackToken.Is("track")) {
    m_ctx.SetErrorF("Expected 'track' in filter condition, got '%s'", trackToken.value.c_str());
    return false;
  }

  if (!tok.Expect(TokenType::DOT)) {
    m_ctx.SetError("Expected '.' after 'track'");
    return false;
  }

  Token field = tok.Next();
  if (field.type != TokenType::IDENTIFIER) {
    m_ctx.SetError("Expected field name after 'track.'");
    return false;
  }

  Token op = tok.Next();
  if (op.type != TokenType::EQUALS_EQUALS) {
    m_ctx.SetError("Expected '==' in filter condition");
    return false;
  }

  Token value = tok.Next();
  if (value.type != TokenType::STRING) {
    m_ctx.SetError("Expected string value in filter condition");
    return false;
  }

  if (!tok.Expect(TokenType::RPAREN)) {
    m_ctx.SetError("Expected ')' after filter condition");
    return false;
  }

  // Execute filter
  if (!FilterTracks(field.value, "==", value.value)) {
    return false;
  }

  m_ctx.in_filter_context = true;

  // Parse method chain (applies to filtered tracks)
  bool result = ParseMethodChain(tok);

  m_ctx.in_filter_context = false;
  m_ctx.filtered_tracks.clear();

  return result;
}

bool Interpreter::ParseMethodChain(Tokenizer &tok) {
  while (tok.Peek().Is(TokenType::DOT)) {
    tok.Next(); // consume '.'

    Token method = tok.Next();
    if (method.type != TokenType::IDENTIFIER) {
      m_ctx.SetError("Expected method name after '.'");
      return false;
    }

    if (!tok.Expect(TokenType::LPAREN)) {
      m_ctx.SetErrorF("Expected '(' after method '%s'", method.value.c_str());
      return false;
    }

    Params params;
    if (!ParseParams(tok, params)) {
      return false;
    }

    if (!tok.Expect(TokenType::RPAREN)) {
      m_ctx.SetError("Expected ')' after method parameters");
      return false;
    }

    // Dispatch to method handler
    bool success = false;
    if (method.value == "new_clip") {
      success = ParseNewClip(tok, params);
    } else if (method.value == "set_track") {
      success = ParseSetTrack(tok, params);
    } else if (method.value == "add_fx") {
      success = ParseAddFx(tok, params);
    } else if (method.value == "addAutomation" || method.value == "add_automation") {
      success = ParseAddAutomation(tok, params);
    } else if (method.value == "delete") {
      success = ParseDelete(tok);
    } else if (method.value == "delete_clip") {
      success = ParseDeleteClip(tok, params);
    } else {
      m_ctx.SetErrorF("Unknown method: %s", method.value.c_str());
      return false;
    }

    if (!success) {
      return false;
    }
  }

  return true;
}

bool Interpreter::ParseParams(Tokenizer &tok, Params &out_params) {
  out_params.Clear();

  // Empty params
  if (tok.Peek().Is(TokenType::RPAREN)) {
    return true;
  }

  while (true) {
    // Parse key
    Token key = tok.Next();
    if (key.type != TokenType::IDENTIFIER) {
      m_ctx.SetErrorF("Expected parameter name, got '%s'", key.value.c_str());
      return false;
    }

    if (!tok.Expect(TokenType::EQUALS)) {
      m_ctx.SetErrorF("Expected '=' after parameter '%s'", key.value.c_str());
      return false;
    }

    // Parse value
    std::string value;
    if (!ParseValue(tok, value)) {
      return false;
    }

    out_params.Set(key.value, value);

    // Check for more parameters
    if (tok.Peek().Is(TokenType::COMMA)) {
      tok.Next(); // consume comma
    } else {
      break;
    }
  }

  return true;
}

bool Interpreter::ParseValue(Tokenizer &tok, std::string &out_value) {
  Token t = tok.Next();

  if (t.type == TokenType::STRING || t.type == TokenType::NUMBER ||
      t.type == TokenType::IDENTIFIER) {
    out_value = t.value;
    return true;
  }

  m_ctx.SetErrorF("Expected value, got '%s'", t.value.c_str());
  return false;
}

// ============================================================================
// Track Operations
// ============================================================================

MediaTrack *Interpreter::CreateTrack(const Params &params) {
  if (!g_rec) {
    m_ctx.SetError("REAPER API not available");
    return nullptr;
  }

  void (*InsertTrackAtIndex)(int, bool) = (void (*)(int, bool))g_rec->GetFunc("InsertTrackAtIndex");
  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *, bool) = (bool (*)(
      MediaTrack *, const char *, char *, bool))g_rec->GetFunc("GetSetMediaTrackInfo_String");
  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))g_rec->GetFunc("TrackFX_AddByName");

  if (!InsertTrackAtIndex || !GetNumTracks || !GetTrack) {
    m_ctx.SetError("Required REAPER API functions not available");
    return nullptr;
  }

  int idx = GetNumTracks();
  InsertTrackAtIndex(idx, false);

  MediaTrack *track = GetTrack(nullptr, idx);
  if (!track) {
    m_ctx.SetError("Failed to create track");
    return nullptr;
  }

  m_ctx.current_track_idx = idx;

  // Set track name if provided
  std::string trackName;
  if (params.Has("name") && GetSetMediaTrackInfo_String) {
    trackName = params.Get("name");
    GetSetMediaTrackInfo_String(track, "P_NAME", (char *)trackName.c_str(), true);
  }

  // Store in global context for Arranger/Drummer to use
  MagdaDSLContext::Get().SetCreatedTrack(idx, trackName.c_str());

  // Add instrument if provided
  if (params.Has("instrument") && TrackFX_AddByName) {
    std::string instrument = params.Get("instrument");
    int fxIdx = TrackFX_AddByName(track, instrument.c_str(), false, -1);
    if (fxIdx < 0) {
      // Try with VST prefix
      std::string vst_name = "VST: " + instrument;
      fxIdx = TrackFX_AddByName(track, vst_name.c_str(), false, -1);
    }
    if (fxIdx < 0) {
      // Log warning but don't fail
      void (*ShowConsoleMsg)(const char *) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
      if (ShowConsoleMsg) {
        char msg[256];
        snprintf(msg, sizeof(msg), "MAGDA DSL: Warning - instrument '%s' not found\n",
                 instrument.c_str());
        ShowConsoleMsg(msg);
      }
    }
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA DSL: Created track %d%s%s\n", idx + 1,
             params.Has("name") ? " named '" : "",
             params.Has("name") ? params.Get("name").c_str() : "");
    ShowConsoleMsg(msg);
  }

  return track;
}

MediaTrack *Interpreter::GetTrackById(int id) {
  if (!g_rec)
    return nullptr;

  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  if (!GetTrack)
    return nullptr;

  // id is 1-based, GetTrack uses 0-based index
  return GetTrack(nullptr, id - 1);
}

MediaTrack *Interpreter::GetTrackByName(const std::string &name) {
  if (!g_rec)
    return nullptr;

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *, bool) = (bool (*)(
      MediaTrack *, const char *, char *, bool))g_rec->GetFunc("GetSetMediaTrackInfo_String");

  if (!GetNumTracks || !GetTrack || !GetSetMediaTrackInfo_String)
    return nullptr;

  int count = GetNumTracks();
  for (int i = 0; i < count; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (track) {
      char track_name[256] = {0};
      GetSetMediaTrackInfo_String(track, "P_NAME", track_name, false);
      if (name == track_name) {
        return track;
      }
    }
  }

  return nullptr;
}

MediaTrack *Interpreter::GetSelectedTrack() {
  if (!g_rec)
    return nullptr;

  MediaTrack *(*GetSelectedTrack2)(void *, int, bool) =
      (MediaTrack * (*)(void *, int, bool)) g_rec->GetFunc("GetSelectedTrack2");
  if (!GetSelectedTrack2)
    return nullptr;

  return GetSelectedTrack2(nullptr, 0, false);
}

void Interpreter::DeleteTrack(MediaTrack *track) {
  if (!g_rec || !track)
    return;

  void (*DeleteTrack)(MediaTrack *) = (void (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
  if (DeleteTrack) {
    DeleteTrack(track);
  }
}

void Interpreter::SetTrackProperties(MediaTrack *track, const Params &params) {
  if (!g_rec || !track)
    return;

  bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *, bool) = (bool (*)(
      MediaTrack *, const char *, char *, bool))g_rec->GetFunc("GetSetMediaTrackInfo_String");
  bool (*SetMediaTrackInfo_Value)(MediaTrack *, const char *, double) =
      (bool (*)(MediaTrack *, const char *, double))g_rec->GetFunc("SetMediaTrackInfo_Value");

  if (params.Has("name") && GetSetMediaTrackInfo_String) {
    std::string name = params.Get("name");
    GetSetMediaTrackInfo_String(track, "P_NAME", (char *)name.c_str(), true);
  }

  if (SetMediaTrackInfo_Value) {
    if (params.Has("volume_db")) {
      double db = params.GetFloat("volume_db");
      double vol = pow(10.0, db / 20.0); // Convert dB to linear
      SetMediaTrackInfo_Value(track, "D_VOL", vol);
    }

    if (params.Has("pan")) {
      double pan = params.GetFloat("pan");
      SetMediaTrackInfo_Value(track, "D_PAN", pan);
    }

    if (params.Has("mute")) {
      SetMediaTrackInfo_Value(track, "B_MUTE", params.GetBool("mute") ? 1.0 : 0.0);
    }

    if (params.Has("solo")) {
      SetMediaTrackInfo_Value(track, "I_SOLO", params.GetBool("solo") ? 1.0 : 0.0);
    }

    if (params.Has("selected")) {
      SetMediaTrackInfo_Value(track, "I_SELECTED", params.GetBool("selected") ? 1.0 : 0.0);
    }
  }
}

// ============================================================================
// Clip Operations
// ============================================================================

double Interpreter::BarsToTime(int bars) const {
  // Convert bars to time (1-based bar number to seconds)
  double bpm = GetProjectBPM();
  double beats_per_bar = GetProjectTimeSignature();
  return (bars - 1) * beats_per_bar * 60.0 / bpm;
}

double Interpreter::BeatsToTime(double beats) const {
  double bpm = GetProjectBPM();
  return beats * 60.0 / bpm;
}

int Interpreter::GetProjectBPM() const {
  if (!g_rec)
    return 120;

  double (*GetProjectTimeSignature2)(void *, double *) =
      (double (*)(void *, double *))g_rec->GetFunc("GetProjectTimeSignature2");
  if (!GetProjectTimeSignature2)
    return 120;

  double bpm = 120.0;
  GetProjectTimeSignature2(nullptr, &bpm);
  return (int)bpm;
}

double Interpreter::GetProjectTimeSignature() const {
  if (!g_rec)
    return 4.0;

  double (*TimeMap_GetTimeSigAtTime)(void *, double, int *, int *, double *) = (double (*)(
      void *, double, int *, int *, double *))g_rec->GetFunc("TimeMap_GetTimeSigAtTime");
  if (!TimeMap_GetTimeSigAtTime)
    return 4.0;

  int num = 4, denom = 4;
  TimeMap_GetTimeSigAtTime(nullptr, 0.0, &num, &denom, nullptr);
  return (double)num;
}

MediaItem *Interpreter::CreateClipAtBar(MediaTrack *track, int bar, int length_bars) {
  if (!g_rec || !track)
    return nullptr;

  MediaItem *(*AddMediaItemToTrack)(MediaTrack *) =
      (MediaItem * (*)(MediaTrack *)) g_rec->GetFunc("AddMediaItemToTrack");
  bool (*SetMediaItemPosition)(MediaItem *, double, bool) =
      (bool (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemPosition");
  bool (*SetMediaItemLength)(MediaItem *, double, bool) =
      (bool (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemLength");
  MediaItem_Take *(*AddTakeToMediaItem)(MediaItem *) =
      (MediaItem_Take * (*)(MediaItem *)) g_rec->GetFunc("AddTakeToMediaItem");
  bool (*SetMediaItemTake_Source)(MediaItem_Take *, void *) =
      (bool (*)(MediaItem_Take *, void *))g_rec->GetFunc("SetMediaItemTake_Source");
  void *(*PCM_Source_CreateFromType)(const char *) =
      (void *(*)(const char *))g_rec->GetFunc("PCM_Source_CreateFromType");

  if (!AddMediaItemToTrack || !SetMediaItemPosition || !SetMediaItemLength) {
    m_ctx.SetError("Required REAPER API functions not available for clip creation");
    return nullptr;
  }

  double pos = BarsToTime(bar);
  double len = BeatsToTime(length_bars * GetProjectTimeSignature());

  MediaItem *item = AddMediaItemToTrack(track);
  if (!item) {
    m_ctx.SetError("Failed to create media item");
    return nullptr;
  }

  SetMediaItemPosition(item, pos, false);
  SetMediaItemLength(item, len, false);

  // Create MIDI take
  if (AddTakeToMediaItem && SetMediaItemTake_Source && PCM_Source_CreateFromType) {
    MediaItem_Take *take = AddTakeToMediaItem(item);
    if (take) {
      void *midi_source = PCM_Source_CreateFromType("MIDI");
      if (midi_source) {
        SetMediaItemTake_Source(take, midi_source);
      }
    }
  }

  m_ctx.current_item = item;

  // Get item index for context
  int (*CountTrackMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");
  int itemIndex = -1;
  if (CountTrackMediaItems) {
    itemIndex = CountTrackMediaItems(track) - 1; // Last item added
  }

  // Store in global context for Arranger/Drummer to use
  MagdaDSLContext::Get().SetCreatedClip(m_ctx.current_track_idx, itemIndex);

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA DSL: Created clip at bar %d, length %d bars\n", bar,
             length_bars);
    ShowConsoleMsg(msg);
  }

  // Update arrange view
  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  if (UpdateArrange) {
    UpdateArrange();
  }

  return item;
}

MediaItem *Interpreter::CreateClipAtPosition(MediaTrack *track, double pos, double length) {
  // Similar to CreateClipAtBar but with time-based positioning
  if (!g_rec || !track)
    return nullptr;

  MediaItem *(*AddMediaItemToTrack)(MediaTrack *) =
      (MediaItem * (*)(MediaTrack *)) g_rec->GetFunc("AddMediaItemToTrack");
  bool (*SetMediaItemPosition)(MediaItem *, double, bool) =
      (bool (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemPosition");
  bool (*SetMediaItemLength)(MediaItem *, double, bool) =
      (bool (*)(MediaItem *, double, bool))g_rec->GetFunc("SetMediaItemLength");

  if (!AddMediaItemToTrack || !SetMediaItemPosition || !SetMediaItemLength) {
    return nullptr;
  }

  MediaItem *item = AddMediaItemToTrack(track);
  if (!item)
    return nullptr;

  SetMediaItemPosition(item, pos, false);
  SetMediaItemLength(item, length, false);

  m_ctx.current_item = item;

  void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
  if (UpdateArrange) {
    UpdateArrange();
  }

  return item;
}

void Interpreter::DeleteClip(MediaTrack *track, int clip_index) {
  if (!g_rec || !track)
    return;

  int (*GetTrackNumMediaItems)(MediaTrack *) =
      (int (*)(MediaTrack *))g_rec->GetFunc("GetTrackNumMediaItems");
  MediaItem *(*GetTrackMediaItem)(MediaTrack *, int) =
      (MediaItem * (*)(MediaTrack *, int)) g_rec->GetFunc("GetTrackMediaItem");
  bool (*DeleteTrackMediaItem)(MediaTrack *, MediaItem *) =
      (bool (*)(MediaTrack *, MediaItem *))g_rec->GetFunc("DeleteTrackMediaItem");

  if (!GetTrackNumMediaItems || !GetTrackMediaItem || !DeleteTrackMediaItem)
    return;

  int count = GetTrackNumMediaItems(track);
  if (clip_index >= 0 && clip_index < count) {
    MediaItem *item = GetTrackMediaItem(track, clip_index);
    if (item) {
      DeleteTrackMediaItem(track, item);
    }
  }
}

void Interpreter::SetClipProperties(MediaItem *item, const Params &params) {
  (void)item;
  (void)params;
  // TODO: Implement clip property setting (color, name, etc.)
}

// ============================================================================
// Method Handlers
// ============================================================================

bool Interpreter::ParseNewClip(Tokenizer &tok, const Params &params) {
  (void)tok; // Not used after params are parsed

  if (!m_ctx.current_track) {
    m_ctx.SetError("No track context for new_clip");
    return false;
  }

  int bar = params.GetInt("bar", 1);
  int length_bars = params.GetInt("length_bars", 4);

  MediaItem *item = CreateClipAtBar(m_ctx.current_track, bar, length_bars);
  return item != nullptr;
}

bool Interpreter::ParseSetTrack(Tokenizer &tok, const Params &params) {
  (void)tok;

  if (m_ctx.in_filter_context) {
    // Apply to all filtered tracks
    for (MediaTrack *track : m_ctx.filtered_tracks) {
      SetTrackProperties(track, params);
    }
  } else if (m_ctx.current_track) {
    SetTrackProperties(m_ctx.current_track, params);
  } else {
    m_ctx.SetError("No track context for set_track");
    return false;
  }

  return true;
}

bool Interpreter::ParseAddFx(Tokenizer &tok, const Params &params) {
  (void)tok;

  if (!m_ctx.current_track) {
    m_ctx.SetError("No track context for add_fx");
    return false;
  }

  std::string fx_name = params.Get("fxname", params.Get("name", ""));
  if (fx_name.empty()) {
    m_ctx.SetError("add_fx requires 'fxname' parameter");
    return false;
  }

  return AddFX(m_ctx.current_track, fx_name);
}

bool Interpreter::AddFX(MediaTrack *track, const std::string &fx_name) {
  if (!g_rec || !track)
    return false;

  int (*TrackFX_AddByName)(MediaTrack *, const char *, bool, int) =
      (int (*)(MediaTrack *, const char *, bool, int))g_rec->GetFunc("TrackFX_AddByName");
  if (!TrackFX_AddByName) {
    m_ctx.SetError("TrackFX_AddByName not available");
    return false;
  }

  int idx = TrackFX_AddByName(track, fx_name.c_str(), false, -1);
  if (idx < 0) {
    // Try with VST prefix
    std::string vst_name = "VST: " + fx_name;
    idx = TrackFX_AddByName(track, vst_name.c_str(), false, -1);
  }

  if (idx < 0) {
    m_ctx.SetErrorF("FX '%s' not found", fx_name.c_str());
    return false;
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA DSL: Added FX '%s' at index %d\n", fx_name.c_str(), idx);
    ShowConsoleMsg(msg);
  }

  return true;
}

bool Interpreter::AddInstrument(MediaTrack *track, const std::string &instrument_name) {
  return AddFX(track, instrument_name);
}

bool Interpreter::ParseAddAutomation(Tokenizer &tok, const Params &params) {
  (void)tok;

  if (!m_ctx.current_track) {
    m_ctx.SetError("No track context for addAutomation");
    return false;
  }

  return AddAutomation(m_ctx.current_track, params);
}

bool Interpreter::AddAutomation(MediaTrack *track, const Params &params) {
  // TODO: Implement automation curve generation
  // This would need to:
  // 1. Get the envelope for the parameter (volume, pan, etc.)
  // 2. Generate curve points based on curve type (fade_in, fade_out, sine,
  // etc.)
  // 3. Insert automation points

  (void)track;

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA DSL: TODO - AddAutomation(param=%s, curve=%s)\n",
             params.Get("param").c_str(), params.Get("curve").c_str());
    ShowConsoleMsg(msg);
  }

  return true;
}

bool Interpreter::ParseDelete(Tokenizer &tok) {
  (void)tok;

  if (m_ctx.in_filter_context) {
    // Delete all filtered tracks
    for (MediaTrack *track : m_ctx.filtered_tracks) {
      DeleteTrack(track);
    }
    m_ctx.filtered_tracks.clear();
  } else if (m_ctx.current_track) {
    DeleteTrack(m_ctx.current_track);
    m_ctx.current_track = nullptr;
    m_ctx.current_track_idx = -1;
  } else {
    m_ctx.SetError("No track context for delete");
    return false;
  }

  return true;
}

bool Interpreter::ParseDeleteClip(Tokenizer &tok, const Params &params) {
  (void)tok;

  if (!m_ctx.current_track) {
    m_ctx.SetError("No track context for delete_clip");
    return false;
  }

  int clip_index = params.GetInt("index", 0);
  DeleteClip(m_ctx.current_track, clip_index);

  return true;
}

// ============================================================================
// Filter Operations
// ============================================================================

bool Interpreter::FilterTracks(const std::string &field, const std::string &op,
                               const std::string &value) {
  if (!g_rec)
    return false;

  int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
  MediaTrack *(*GetTrack)(void *, int) = (MediaTrack * (*)(void *, int)) g_rec->GetFunc("GetTrack");
  bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *, bool) = (bool (*)(
      MediaTrack *, const char *, char *, bool))g_rec->GetFunc("GetSetMediaTrackInfo_String");

  if (!GetNumTracks || !GetTrack || !GetSetMediaTrackInfo_String)
    return false;

  m_ctx.filtered_tracks.clear();

  int count = GetNumTracks();
  for (int i = 0; i < count; i++) {
    MediaTrack *track = GetTrack(nullptr, i);
    if (!track)
      continue;

    bool matches = false;

    if (field == "name") {
      char track_name[256] = {0};
      GetSetMediaTrackInfo_String(track, "P_NAME", track_name, false);

      if (op == "==") {
        matches = (value == track_name);
      }
    }
    // Add more fields as needed (index, color, etc.)

    if (matches) {
      m_ctx.filtered_tracks.push_back(track);
    }
  }

  void (*ShowConsoleMsg)(const char *) = (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (ShowConsoleMsg) {
    char msg[256];
    snprintf(msg, sizeof(msg), "MAGDA DSL: Filter matched %d tracks (field=%s, op=%s, value=%s)\n",
             (int)m_ctx.filtered_tracks.size(), field.c_str(), op.c_str(), value.c_str());
    ShowConsoleMsg(msg);
  }

  return true;
}

bool Interpreter::ApplyToFilteredTracks(const std::string &method, const Params &params) {
  // This is handled by ParseSetTrack checking m_ctx.in_filter_context
  (void)method;
  (void)params;
  return true;
}

} // namespace MagdaDSL
