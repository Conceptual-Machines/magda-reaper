/**
 * Unit tests for MAGDA DSL Tokenizer and Params
 *
 * Tests DSL parsing without requiring REAPER runtime.
 * These tests can run in CI environments.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>

// Include WDL string first
#include "WDL/wdlstring.h"

// We'll test the Tokenizer and Params classes directly
// These don't depend on REAPER

namespace MagdaDSL {

// ============================================================================
// Token Types (copied from magda_dsl_interpreter.h for standalone testing)
// ============================================================================
enum class TokenType {
    IDENTIFIER,
    STRING,
    NUMBER,
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,
    DOT,
    COMMA,
    EQUALS,
    EQUALS_EQUALS,
    SEMICOLON,
    AT,
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
// Tokenizer (copied for standalone testing)
// ============================================================================
class Tokenizer {
public:
    explicit Tokenizer(const char* input)
        : m_input(input)
        , m_pos(input)
        , m_line(1)
        , m_col(1)
        , m_has_peeked(false)
    {}

    Token Next() {
        if (m_has_peeked) {
            m_has_peeked = false;
            return m_peeked;
        }
        return ReadToken();
    }

    Token Peek() {
        if (!m_has_peeked) {
            m_peeked = ReadToken();
            m_has_peeked = true;
        }
        return m_peeked;
    }

    bool HasMore() const {
        return *m_pos != '\0' || m_has_peeked;
    }

private:
    Token ReadToken() {
        SkipWhitespace();

        if (*m_pos == '\0') {
            return Token(TokenType::END_OF_INPUT, "", m_line, m_col);
        }

        int start_col = m_col;

        // Single-character tokens
        switch (*m_pos) {
            case '(': m_pos++; m_col++; return Token(TokenType::LPAREN, "(", m_line, start_col);
            case ')': m_pos++; m_col++; return Token(TokenType::RPAREN, ")", m_line, start_col);
            case '[': m_pos++; m_col++; return Token(TokenType::LBRACKET, "[", m_line, start_col);
            case ']': m_pos++; m_col++; return Token(TokenType::RBRACKET, "]", m_line, start_col);
            case '{': m_pos++; m_col++; return Token(TokenType::LBRACE, "{", m_line, start_col);
            case '}': m_pos++; m_col++; return Token(TokenType::RBRACE, "}", m_line, start_col);
            case '.': m_pos++; m_col++; return Token(TokenType::DOT, ".", m_line, start_col);
            case ',': m_pos++; m_col++; return Token(TokenType::COMMA, ",", m_line, start_col);
            case ';': m_pos++; m_col++; return Token(TokenType::SEMICOLON, ";", m_line, start_col);
            case '@': m_pos++; m_col++; return Token(TokenType::AT, "@", m_line, start_col);
            case '=':
                m_pos++; m_col++;
                if (*m_pos == '=') {
                    m_pos++; m_col++;
                    return Token(TokenType::EQUALS_EQUALS, "==", m_line, start_col);
                }
                return Token(TokenType::EQUALS, "=", m_line, start_col);
        }

        // String
        if (*m_pos == '"') {
            return ReadString();
        }

        // Number (including negative)
        if (isdigit(*m_pos) || (*m_pos == '-' && isdigit(*(m_pos + 1)))) {
            return ReadNumber();
        }

        // Identifier
        if (isalpha(*m_pos) || *m_pos == '_') {
            return ReadIdentifier();
        }

        // Unknown character - error
        m_error.SetFormatted(64, "Unexpected character '%c' at line %d col %d", *m_pos, m_line, m_col);
        m_pos++;
        m_col++;
        return Token(TokenType::ERROR, m_error.Get(), m_line, start_col);
    }

    void SkipWhitespace() {
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
                while (*m_pos && *m_pos != '\n') {
                    m_pos++;
                }
            } else if (*m_pos == '#') {
                // Also skip # comments
                while (*m_pos && *m_pos != '\n') {
                    m_pos++;
                }
            } else {
                break;
            }
        }
    }

    Token ReadIdentifier() {
        int start_col = m_col;
        const char* start = m_pos;

        while (*m_pos && (isalnum(*m_pos) || *m_pos == '_')) {
            m_pos++;
            m_col++;
        }

        return Token(TokenType::IDENTIFIER, std::string(start, m_pos - start), m_line, start_col);
    }

    Token ReadString() {
        int start_col = m_col;
        m_pos++; // Skip opening quote
        m_col++;

        std::string value;
        while (*m_pos && *m_pos != '"') {
            if (*m_pos == '\\' && *(m_pos + 1)) {
                m_pos++;
                m_col++;
                switch (*m_pos) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    default: value += *m_pos; break;
                }
            } else {
                value += *m_pos;
            }
            m_pos++;
            m_col++;
        }

        if (*m_pos == '"') {
            m_pos++;
            m_col++;
        }

        return Token(TokenType::STRING, value, m_line, start_col);
    }

    Token ReadNumber() {
        int start_col = m_col;
        const char* start = m_pos;

        if (*m_pos == '-') {
            m_pos++;
            m_col++;
        }

        while (isdigit(*m_pos)) {
            m_pos++;
            m_col++;
        }

        if (*m_pos == '.') {
            m_pos++;
            m_col++;
            while (isdigit(*m_pos)) {
                m_pos++;
                m_col++;
            }
        }

        return Token(TokenType::NUMBER, std::string(start, m_pos - start), m_line, start_col);
    }

    const char* m_input;
    const char* m_pos;
    int m_line;
    int m_col;
    Token m_peeked;
    bool m_has_peeked;
    WDL_FastString m_error;
};

// ============================================================================
// Params (copied for standalone testing)
// ============================================================================
class Params {
public:
    void Set(const std::string& key, const std::string& value) {
        m_params[key] = value;
    }

    void SetInt(const std::string& key, int value) {
        m_params[key] = std::to_string(value);
    }

    void SetFloat(const std::string& key, double value) {
        m_params[key] = std::to_string(value);
    }

    void SetBool(const std::string& key, bool value) {
        m_params[key] = value ? "true" : "false";
    }

    bool Has(const std::string& key) const {
        return m_params.find(key) != m_params.end();
    }

    std::string Get(const std::string& key, const std::string& def = "") const {
        auto it = m_params.find(key);
        return (it != m_params.end()) ? it->second : def;
    }

    int GetInt(const std::string& key, int def = 0) const {
        auto it = m_params.find(key);
        if (it == m_params.end()) return def;
        return atoi(it->second.c_str());
    }

    double GetFloat(const std::string& key, double def = 0.0) const {
        auto it = m_params.find(key);
        if (it == m_params.end()) return def;
        return atof(it->second.c_str());
    }

    bool GetBool(const std::string& key, bool def = false) const {
        auto it = m_params.find(key);
        if (it == m_params.end()) return def;
        return it->second == "true" || it->second == "True" || it->second == "1";
    }

    void Clear() { m_params.clear(); }
    bool Empty() const { return m_params.empty(); }

private:
    std::map<std::string, std::string> m_params;
};

} // namespace MagdaDSL

using namespace MagdaDSL;

// ============================================================================
// Tokenizer Tests
// ============================================================================

class TokenizerTest : public ::testing::Test {};

TEST_F(TokenizerTest, TokenizeIdentifier) {
    Tokenizer tok("track");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::IDENTIFIER);
    EXPECT_EQ(t.value, "track");
}

TEST_F(TokenizerTest, TokenizeString) {
    Tokenizer tok("\"hello world\"");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::STRING);
    EXPECT_EQ(t.value, "hello world");
}

TEST_F(TokenizerTest, TokenizeStringWithEscapes) {
    Tokenizer tok("\"hello\\nworld\"");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::STRING);
    EXPECT_EQ(t.value, "hello\nworld");
}

TEST_F(TokenizerTest, TokenizeNumber) {
    Tokenizer tok("42");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::NUMBER);
    EXPECT_EQ(t.value, "42");
}

TEST_F(TokenizerTest, TokenizeNegativeNumber) {
    Tokenizer tok("-6.5");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::NUMBER);
    EXPECT_EQ(t.value, "-6.5");
}

TEST_F(TokenizerTest, TokenizeFloat) {
    Tokenizer tok("3.14159");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::NUMBER);
    EXPECT_EQ(t.value, "3.14159");
}

TEST_F(TokenizerTest, TokenizePunctuation) {
    Tokenizer tok("().=,");

    EXPECT_EQ(tok.Next().type, TokenType::LPAREN);
    EXPECT_EQ(tok.Next().type, TokenType::RPAREN);
    EXPECT_EQ(tok.Next().type, TokenType::DOT);
    EXPECT_EQ(tok.Next().type, TokenType::EQUALS);
    EXPECT_EQ(tok.Next().type, TokenType::COMMA);
}

TEST_F(TokenizerTest, TokenizeEqualsEquals) {
    Tokenizer tok("==");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::EQUALS_EQUALS);
    EXPECT_EQ(t.value, "==");
}

TEST_F(TokenizerTest, TokenizeCreateTrack) {
    Tokenizer tok("create_track(name=\"Bass\", index=0)");

    Token t1 = tok.Next();
    EXPECT_EQ(t1.type, TokenType::IDENTIFIER);
    EXPECT_EQ(t1.value, "create_track");

    EXPECT_EQ(tok.Next().type, TokenType::LPAREN);

    Token t3 = tok.Next();
    EXPECT_EQ(t3.type, TokenType::IDENTIFIER);
    EXPECT_EQ(t3.value, "name");

    EXPECT_EQ(tok.Next().type, TokenType::EQUALS);

    Token t5 = tok.Next();
    EXPECT_EQ(t5.type, TokenType::STRING);
    EXPECT_EQ(t5.value, "Bass");

    EXPECT_EQ(tok.Next().type, TokenType::COMMA);

    Token t7 = tok.Next();
    EXPECT_EQ(t7.type, TokenType::IDENTIFIER);
    EXPECT_EQ(t7.value, "index");

    EXPECT_EQ(tok.Next().type, TokenType::EQUALS);

    Token t9 = tok.Next();
    EXPECT_EQ(t9.type, TokenType::NUMBER);
    EXPECT_EQ(t9.value, "0");

    EXPECT_EQ(tok.Next().type, TokenType::RPAREN);
}

TEST_F(TokenizerTest, SkipComments) {
    Tokenizer tok("track // this is a comment\nfilter");

    Token t1 = tok.Next();
    EXPECT_EQ(t1.value, "track");

    Token t2 = tok.Next();
    EXPECT_EQ(t2.value, "filter");
}

TEST_F(TokenizerTest, SkipHashComments) {
    Tokenizer tok("track # this is a comment\nfilter");

    Token t1 = tok.Next();
    EXPECT_EQ(t1.value, "track");

    Token t2 = tok.Next();
    EXPECT_EQ(t2.value, "filter");
}

TEST_F(TokenizerTest, HandleEmptyInput) {
    Tokenizer tok("");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::END_OF_INPUT);
}

TEST_F(TokenizerTest, HandleWhitespaceOnly) {
    Tokenizer tok("   \t\n  ");
    Token t = tok.Next();

    EXPECT_EQ(t.type, TokenType::END_OF_INPUT);
}

TEST_F(TokenizerTest, PeekDoesNotConsume) {
    Tokenizer tok("track filter");

    Token peek1 = tok.Peek();
    EXPECT_EQ(peek1.value, "track");

    Token peek2 = tok.Peek();
    EXPECT_EQ(peek2.value, "track");

    Token next = tok.Next();
    EXPECT_EQ(next.value, "track");

    Token next2 = tok.Next();
    EXPECT_EQ(next2.value, "filter");
}

TEST_F(TokenizerTest, LineTracking) {
    Tokenizer tok("first\nsecond\nthird");

    Token t1 = tok.Next();
    EXPECT_EQ(t1.line, 1);

    Token t2 = tok.Next();
    EXPECT_EQ(t2.line, 2);

    Token t3 = tok.Next();
    EXPECT_EQ(t3.line, 3);
}

TEST_F(TokenizerTest, ColumnTracking) {
    Tokenizer tok("ab cd");

    Token t1 = tok.Next();
    EXPECT_EQ(t1.col, 1);

    Token t2 = tok.Next();
    EXPECT_EQ(t2.col, 4);
}

TEST_F(TokenizerTest, TokenizeMethodChain) {
    Tokenizer tok("track(1).set(volume=-6.0)");

    EXPECT_EQ(tok.Next().value, "track");
    EXPECT_EQ(tok.Next().type, TokenType::LPAREN);
    EXPECT_EQ(tok.Next().value, "1");
    EXPECT_EQ(tok.Next().type, TokenType::RPAREN);
    EXPECT_EQ(tok.Next().type, TokenType::DOT);
    EXPECT_EQ(tok.Next().value, "set");
    EXPECT_EQ(tok.Next().type, TokenType::LPAREN);
    EXPECT_EQ(tok.Next().value, "volume");
    EXPECT_EQ(tok.Next().type, TokenType::EQUALS);
    EXPECT_EQ(tok.Next().value, "-6.0");
    EXPECT_EQ(tok.Next().type, TokenType::RPAREN);
}

TEST_F(TokenizerTest, TokenizeBrackets) {
    Tokenizer tok("[1, 2, 3]");

    EXPECT_EQ(tok.Next().type, TokenType::LBRACKET);
    EXPECT_EQ(tok.Next().value, "1");
    EXPECT_EQ(tok.Next().type, TokenType::COMMA);
    EXPECT_EQ(tok.Next().value, "2");
    EXPECT_EQ(tok.Next().type, TokenType::COMMA);
    EXPECT_EQ(tok.Next().value, "3");
    EXPECT_EQ(tok.Next().type, TokenType::RBRACKET);
}

TEST_F(TokenizerTest, TokenizeBraces) {
    Tokenizer tok("{key: value}");

    EXPECT_EQ(tok.Next().type, TokenType::LBRACE);
    EXPECT_EQ(tok.Next().value, "key");
}

// ============================================================================
// Params Tests
// ============================================================================

class ParamsTest : public ::testing::Test {};

TEST_F(ParamsTest, SetAndGet) {
    Params p;
    p.Set("name", "Bass");

    EXPECT_TRUE(p.Has("name"));
    EXPECT_EQ(p.Get("name"), "Bass");
}

TEST_F(ParamsTest, GetDefault) {
    Params p;

    EXPECT_FALSE(p.Has("missing"));
    EXPECT_EQ(p.Get("missing", "default"), "default");
}

TEST_F(ParamsTest, SetAndGetInt) {
    Params p;
    p.SetInt("index", 42);

    EXPECT_EQ(p.GetInt("index"), 42);
}

TEST_F(ParamsTest, GetIntDefault) {
    Params p;

    EXPECT_EQ(p.GetInt("missing", -1), -1);
}

TEST_F(ParamsTest, SetAndGetFloat) {
    Params p;
    p.SetFloat("volume", -6.5);

    EXPECT_DOUBLE_EQ(p.GetFloat("volume"), -6.5);
}

TEST_F(ParamsTest, GetFloatDefault) {
    Params p;

    EXPECT_DOUBLE_EQ(p.GetFloat("missing", 1.0), 1.0);
}

TEST_F(ParamsTest, SetAndGetBoolTrue) {
    Params p;
    p.SetBool("mute", true);

    EXPECT_TRUE(p.GetBool("mute"));
}

TEST_F(ParamsTest, SetAndGetBoolFalse) {
    Params p;
    p.SetBool("mute", false);

    EXPECT_FALSE(p.GetBool("mute"));
}

TEST_F(ParamsTest, GetBoolDefault) {
    Params p;

    EXPECT_TRUE(p.GetBool("missing", true));
    EXPECT_FALSE(p.GetBool("missing", false));
}

TEST_F(ParamsTest, GetBoolFromString) {
    Params p;
    p.Set("a", "true");
    p.Set("b", "True");
    p.Set("c", "1");
    p.Set("d", "false");

    EXPECT_TRUE(p.GetBool("a"));
    EXPECT_TRUE(p.GetBool("b"));
    EXPECT_TRUE(p.GetBool("c"));
    EXPECT_FALSE(p.GetBool("d"));
}

TEST_F(ParamsTest, Clear) {
    Params p;
    p.Set("a", "1");
    p.Set("b", "2");

    EXPECT_FALSE(p.Empty());

    p.Clear();

    EXPECT_TRUE(p.Empty());
    EXPECT_FALSE(p.Has("a"));
}

TEST_F(ParamsTest, ParseIntFromString) {
    Params p;
    p.Set("index", "42");

    EXPECT_EQ(p.GetInt("index"), 42);
}

TEST_F(ParamsTest, ParseFloatFromString) {
    Params p;
    p.Set("volume", "-12.5");

    EXPECT_DOUBLE_EQ(p.GetFloat("volume"), -12.5);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
