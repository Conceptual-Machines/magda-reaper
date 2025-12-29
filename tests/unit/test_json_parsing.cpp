/**
 * Unit tests for WDL JSON parser
 *
 * Tests JSON parsing functionality used by MAGDA.
 * These tests can run in CI environments.
 */

#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include "WDL/jsonparse.h"

// ============================================================================
// JSON Parsing Tests
// ============================================================================

class JSONParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic object parsing
TEST_F(JSONParserTest, ParseSimpleObject) {
    const char* json = R"({"name": "test", "value": 42})";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);
    EXPECT_EQ(parser.m_err, nullptr);
    EXPECT_TRUE(root->is_object());

    wdl_json_element* name = root->get_item_by_name("name");
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name->get_string_value(), "test");

    wdl_json_element* value = root->get_item_by_name("value");
    ASSERT_NE(value, nullptr);
    // Numeric values are not strings
    EXPECT_EQ(value->get_string_value(), nullptr);
    // But can be accessed with allow_unquoted
    EXPECT_STREQ(value->get_string_value(true), "42");
}

// Test array parsing
TEST_F(JSONParserTest, ParseArray) {
    const char* json = R"([1, 2, 3, 4, 5])";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->is_array());
    EXPECT_FALSE(root->is_object());

    int count = root->m_array ? root->m_array->GetSize() : 0;
    EXPECT_EQ(count, 5);

    // First element should be "1"
    wdl_json_element* first = root->enum_item(0);
    ASSERT_NE(first, nullptr);
    EXPECT_STREQ(first->get_string_value(true), "1");
}

// Test nested objects
TEST_F(JSONParserTest, ParseNestedObject) {
    const char* json = R"({
        "track": {
            "name": "Bass",
            "volume": -6.0,
            "plugins": ["ReaEQ", "ReaComp"]
        }
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->is_object());

    wdl_json_element* track = root->get_item_by_name("track");
    ASSERT_NE(track, nullptr);
    EXPECT_TRUE(track->is_object());

    EXPECT_STREQ(track->get_string_by_name("name"), "Bass");

    wdl_json_element* plugins = track->get_item_by_name("plugins");
    ASSERT_NE(plugins, nullptr);
    EXPECT_TRUE(plugins->is_array());
    EXPECT_EQ(plugins->m_array->GetSize(), 2);
}

// Test boolean values
TEST_F(JSONParserTest, ParseBooleans) {
    const char* json = R"({"enabled": true, "disabled": false})";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    wdl_json_element* enabled = root->get_item_by_name("enabled");
    ASSERT_NE(enabled, nullptr);
    EXPECT_STREQ(enabled->get_string_value(true), "true");

    wdl_json_element* disabled = root->get_item_by_name("disabled");
    ASSERT_NE(disabled, nullptr);
    EXPECT_STREQ(disabled->get_string_value(true), "false");
}

// Test null value
TEST_F(JSONParserTest, ParseNull) {
    const char* json = R"({"value": null})";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    wdl_json_element* value = root->get_item_by_name("value");
    ASSERT_NE(value, nullptr);
    EXPECT_STREQ(value->get_string_value(true), "null");
}

// Test empty object
TEST_F(JSONParserTest, ParseEmptyObject) {
    const char* json = "{}";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->is_object());
}

// Test empty array
TEST_F(JSONParserTest, ParseEmptyArray) {
    const char* json = "[]";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->is_array());
}

// Test action JSON format (MAGDA specific)
TEST_F(JSONParserTest, ParseActionJSON) {
    const char* json = R"({
        "action": "create_track",
        "params": {
            "name": "Drums",
            "index": 0
        }
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(root->get_string_by_name("action"), "create_track");

    wdl_json_element* params = root->get_item_by_name("params");
    ASSERT_NE(params, nullptr);
    EXPECT_TRUE(params->is_object());
    EXPECT_STREQ(params->get_string_by_name("name"), "Drums");
}

// Test SSE event JSON format
TEST_F(JSONParserTest, ParseSSEEventJSON) {
    const char* json = R"({
        "type": "chunk",
        "chunk": "desc:Test Effect\n"
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(root->get_string_by_name("type"), "chunk");

    const char* chunk = root->get_string_by_name("chunk");
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(strstr(chunk, "desc:Test Effect") != nullptr);
}

// Test OpenAI streaming event format
TEST_F(JSONParserTest, ParseOpenAIStreamEvent) {
    const char* json = R"({
        "type": "response.output_text.delta",
        "delta": "Hello"
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(root->get_string_by_name("type"), "response.output_text.delta");
    EXPECT_STREQ(root->get_string_by_name("delta"), "Hello");
}

// Test OpenAI response.done event
TEST_F(JSONParserTest, ParseOpenAIResponseDone) {
    const char* json = R"({
        "type": "response.done",
        "response": {
            "id": "resp_123",
            "status": "completed"
        }
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(root->get_string_by_name("type"), "response.done");

    wdl_json_element* response = root->get_item_by_name("response");
    ASSERT_NE(response, nullptr);
    EXPECT_STREQ(response->get_string_by_name("status"), "completed");
}

// Test string with special characters
TEST_F(JSONParserTest, ParseStringWithNewlines) {
    const char* json = R"({"code": "line1\nline2\nline3"})";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    const char* code = root->get_string_by_name("code");
    ASSERT_NE(code, nullptr);
    // Should contain actual newlines
    EXPECT_TRUE(strchr(code, '\n') != nullptr);
}

// Test floating point numbers
TEST_F(JSONParserTest, ParseFloatingPoint) {
    const char* json = R"({"volume": -6.5, "pan": 0.25, "tempo": 120.0})";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    wdl_json_element* volume = root->get_item_by_name("volume");
    ASSERT_NE(volume, nullptr);
    EXPECT_STREQ(volume->get_string_value(true), "-6.5");

    wdl_json_element* pan = root->get_item_by_name("pan");
    ASSERT_NE(pan, nullptr);
    // Check it parsed as number
    double pan_val = atof(pan->get_string_value(true));
    EXPECT_DOUBLE_EQ(pan_val, 0.25);
}

// Test MAGDA actions array format
TEST_F(JSONParserTest, ParseActionsArray) {
    const char* json = R"({
        "actions": [
            {"action": "create_track", "name": "Drums"},
            {"action": "create_track", "name": "Bass"},
            {"action": "set_volume", "track": 0, "volume": -6.0}
        ]
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    wdl_json_element* actions = root->get_item_by_name("actions");
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->is_array());
    EXPECT_EQ(actions->m_array->GetSize(), 3);

    wdl_json_element* first = actions->enum_item(0);
    ASSERT_NE(first, nullptr);
    EXPECT_STREQ(first->get_string_by_name("action"), "create_track");
    EXPECT_STREQ(first->get_string_by_name("name"), "Drums");
}

// Test MIDI notes format
TEST_F(JSONParserTest, ParseMIDINotes) {
    const char* json = R"({
        "notes": [
            {"note": 60, "velocity": 100, "start": 0.0, "length": 0.5},
            {"note": 64, "velocity": 90, "start": 0.5, "length": 0.5},
            {"note": 67, "velocity": 85, "start": 1.0, "length": 1.0}
        ]
    })";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    ASSERT_NE(root, nullptr);

    wdl_json_element* notes = root->get_item_by_name("notes");
    ASSERT_NE(notes, nullptr);
    EXPECT_TRUE(notes->is_array());
    EXPECT_EQ(notes->m_array->GetSize(), 3);

    wdl_json_element* first_note = notes->enum_item(0);
    ASSERT_NE(first_note, nullptr);

    // Check note value
    const char* note_str = first_note->get_item_by_name("note")->get_string_value(true);
    EXPECT_STREQ(note_str, "60");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(JSONParserTest, HandleInvalidJSON) {
    const char* json = "{ invalid json }";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, strlen(json));

    // WDL JSON5 parser is lenient - it might parse or set error
    // Just ensure it doesn't crash
    SUCCEED();
}

TEST_F(JSONParserTest, HandleEmptyInput) {
    const char* json = "";

    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(json, 0);

    // Empty input should be handled gracefully
    SUCCEED();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
