#ifndef MAGDA_DSL_INTERPRETER_H
#define MAGDA_DSL_INTERPRETER_H

#include "../WDL/WDL/wdlstring.h"
#include <vector>
#include <map>
#include <string>

// Forward declarations - use 'class' to match REAPER SDK
class MediaTrack;
class MediaItem;

namespace MagdaDSL {

// ============================================================================
// Token Types
// ============================================================================
enum class TokenType {
    IDENTIFIER,     // track, filter, new_clip, etc.
    STRING,         // "Serum", "Bass"
    NUMBER,         // 3, 4.5, -6.0
    LPAREN,         // (
    RPAREN,         // )
    LBRACKET,       // [
    RBRACKET,       // ]
    LBRACE,         // {
    RBRACE,         // }
    DOT,            // .
    COMMA,          // ,
    EQUALS,         // =
    EQUALS_EQUALS,  // ==
    SEMICOLON,      // ;
    AT,             // @
    END_OF_INPUT,
    ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;

    Token() : type(TokenType::END_OF_INPUT), line(0), col(0) {}
    Token(TokenType t, const std::string& v, int l = 0, int c = 0)
        : type(t), value(v), line(l), col(c) {}

    bool Is(TokenType t) const { return type == t; }
    bool Is(const char* id) const { return type == TokenType::IDENTIFIER && value == id; }
};

// ============================================================================
// Tokenizer
// ============================================================================
class Tokenizer {
public:
    explicit Tokenizer(const char* input);

    Token Next();
    Token Peek();
    bool HasMore() const;

    // Consume expected token, return false if mismatch
    bool Expect(TokenType type);
    bool Expect(const char* identifier);

    const char* GetError() const { return m_error.Get(); }

private:
    void SkipWhitespace();
    void SkipComment();
    Token ReadIdentifier();
    Token ReadString();
    Token ReadNumber();

    const char* m_input;
    const char* m_pos;
    int m_line;
    int m_col;
    Token m_peeked;
    bool m_has_peeked;
    WDL_FastString m_error;
};

// ============================================================================
// Parameter Map (for function arguments)
// ============================================================================
class Params {
public:
    void Set(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetFloat(const std::string& key, double value);
    void SetBool(const std::string& key, bool value);

    bool Has(const std::string& key) const;
    std::string Get(const std::string& key, const std::string& def = "") const;
    int GetInt(const std::string& key, int def = 0) const;
    double GetFloat(const std::string& key, double def = 0.0) const;
    bool GetBool(const std::string& key, bool def = false) const;

    void Clear() { m_params.clear(); }
    bool Empty() const { return m_params.empty(); }

private:
    std::map<std::string, std::string> m_params;
};

// ============================================================================
// Interpreter Context (tracks execution state)
// ============================================================================
struct InterpreterContext {
    MediaTrack* current_track = nullptr;
    int current_track_idx = -1;
    MediaItem* current_item = nullptr;

    // For filter operations
    std::vector<MediaTrack*> filtered_tracks;
    bool in_filter_context = false;

    // Error handling
    WDL_FastString error;
    bool has_error = false;

    void SetError(const char* msg) {
        error.Set(msg);
        has_error = true;
    }

    void SetErrorF(const char* fmt, ...);
};

// ============================================================================
// DSL Interpreter - Parses and executes DSL directly
// ============================================================================
class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // Main entry point - execute DSL code
    bool Execute(const char* dsl_code);

    // Get error message if Execute returns false
    const char* GetError() const { return m_ctx.error.Get(); }

    // Set current REAPER project state (for track lookups)
    void SetState(const std::map<std::string, std::string>& state);

private:
    // Statement parsing
    bool ParseStatement(Tokenizer& tok);
    bool ParseTrackStatement(Tokenizer& tok);
    bool ParseFilterStatement(Tokenizer& tok);

    // Chain method parsing
    bool ParseMethodChain(Tokenizer& tok);
    bool ParseNewClip(Tokenizer& tok, const Params& params);
    bool ParseSetTrack(Tokenizer& tok, const Params& params);
    bool ParseAddFx(Tokenizer& tok, const Params& params);
    bool ParseAddAutomation(Tokenizer& tok, const Params& params);
    bool ParseDelete(Tokenizer& tok);
    bool ParseDeleteClip(Tokenizer& tok, const Params& params);

    // Parameter parsing
    bool ParseParams(Tokenizer& tok, Params& out_params);
    bool ParseValue(Tokenizer& tok, std::string& out_value);

    // Track operations (direct REAPER API calls)
    MediaTrack* CreateTrack(const Params& params);
    MediaTrack* GetTrackById(int id);  // 1-based
    MediaTrack* GetTrackByName(const std::string& name);
    MediaTrack* GetSelectedTrack();
    void DeleteTrack(MediaTrack* track);
    void SetTrackProperties(MediaTrack* track, const Params& params);

    // Clip operations
    MediaItem* CreateClipAtBar(MediaTrack* track, int bar, int length_bars);
    MediaItem* CreateClipAtPosition(MediaTrack* track, double pos, double length);
    void DeleteClip(MediaTrack* track, int clip_index);
    void SetClipProperties(MediaItem* item, const Params& params);

    // FX operations
    bool AddFX(MediaTrack* track, const std::string& fx_name);
    bool AddInstrument(MediaTrack* track, const std::string& instrument_name);

    // Automation operations
    bool AddAutomation(MediaTrack* track, const Params& params);

    // Filter operations
    bool FilterTracks(const std::string& field, const std::string& op, const std::string& value);
    bool ApplyToFilteredTracks(const std::string& method, const Params& params);

    // Utility
    double BarsToTime(int bars) const;
    double BeatsToTime(double beats) const;
    int GetProjectBPM() const;
    double GetProjectTimeSignature() const;

    InterpreterContext m_ctx;
};

} // namespace MagdaDSL

#endif // MAGDA_DSL_INTERPRETER_H
