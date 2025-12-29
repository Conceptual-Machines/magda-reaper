#include "magda_executor.h"
#include "magda_actions.h"
#include "reaper_plugin.h"
#include <cstring>

bool MagdaExecutor::ExecuteAction(const char *input, WDL_FastString &error_msg) {
  if (!input || !input[0]) {
    error_msg.Set("Empty input");
    return false;
  }

  // Get the REAPER API pointer from main.cpp
  extern reaper_plugin_info_t *g_rec;
  if (!g_rec) {
    error_msg.Set("REAPER API not available");
    return false;
  }

  // TODO: For now, we'll parse the input as JSON directly
  // Later, we'll call the HTTP API to get structured output
  // For testing, you can pass JSON directly like:
  // {"action": "create_track", "name": "Test"}
  // or an array: [{"action": "create_track", "name": "Test"}]

  // Check if input looks like JSON (starts with { or [)
  if (input[0] == '{' || input[0] == '[') {
    // Input is JSON - parse and execute directly
    WDL_FastString result;
    if (MagdaActions::ExecuteActions(input, result, error_msg)) {
      return true;
    }
    return false;
  }

  // Otherwise, treat as plain text and create a track with that name
  // This is a fallback for simple text input
  // ExecuteActions expects either an array or a single action object
  // We'll use a single action object format
  WDL_FastString json_action;
  json_action.Append("{\"action\":\"create_track\",\"name\":\"");
  // Escape quotes in the name
  for (const char *p = input; *p; p++) {
    if (*p == '"' || *p == '\\') {
      json_action.Append("\\");
    }
    json_action.AppendFormatted(1, "%c", *p);
  }
  json_action.Append("\"}");

  WDL_FastString result;
  if (MagdaActions::ExecuteActions(json_action.Get(), result, error_msg)) {
    return true;
  }
  return false;
}
