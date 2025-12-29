#include "magda_drummer_interpreter.h"
#include "magda_actions.h"
#include "magda_dsl_context.h"
#include "reaper_plugin.h"
#include <cstdlib>
#include <cstring>
#include <string>

extern reaper_plugin_info_t *g_rec;

namespace MagdaDrummer {

static void Log(const char *fmt, ...) {
  if (!g_rec)
    return;
  void (*ShowConsoleMsg)(const char *) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
  if (!ShowConsoleMsg)
    return;

  char msg[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  ShowConsoleMsg(msg);
}

Interpreter::Interpreter() : m_trackIndex(-1) {}
Interpreter::~Interpreter() {}

bool Interpreter::Execute(const char *dsl_code) {
  if (!dsl_code || !*dsl_code) {
    m_error.Set("Empty DSL code");
    return false;
  }

  Log("MAGDA Drummer: Executing: %s\n", dsl_code);

  // Skip whitespace
  const char *pos = dsl_code;
  while (*pos && (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t'))
    pos++;

  // Handle multiple patterns separated by semicolons
  std::string code(pos);
  size_t start = 0;
  bool allSuccess = true;

  while (start < code.length()) {
    // Find end of this pattern call
    size_t end = code.find(';', start);
    if (end == std::string::npos)
      end = code.length();

    std::string pattern = code.substr(start, end - start);

    // Trim
    size_t pstart = pattern.find_first_not_of(" \t\r\n");
    if (pstart != std::string::npos) {
      size_t pend = pattern.find_last_not_of(" \t\r\n");
      pattern = pattern.substr(pstart, pend - pstart + 1);
    }

    if (!pattern.empty() && pattern.find("pattern(") == 0) {
      if (!ExecutePattern(pattern.c_str() + 8)) {
        allSuccess = false;
      }
    }

    start = end + 1;
  }

  return allSuccess;
}

bool Interpreter::ExecutePattern(const char *params) {
  // Parse: drum=kick, grid="x---x---", velocity=100
  std::string paramStr(params);

  // Remove trailing )
  size_t endPos = paramStr.find(')');
  if (endPos != std::string::npos) {
    paramStr = paramStr.substr(0, endPos);
  }

  std::string drumName;
  std::string grid;
  int velocity = 100;

  // Parse key=value pairs
  size_t pos = 0;
  while (pos < paramStr.length()) {
    while (pos < paramStr.length() &&
           (paramStr[pos] == ' ' || paramStr[pos] == ','))
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
    } else {
      size_t end = paramStr.find_first_of(", )", pos);
      if (end == std::string::npos)
        end = paramStr.length();
      value = paramStr.substr(pos, end - pos);
      pos = end;
    }

    if (key == "drum")
      drumName = value;
    else if (key == "grid")
      grid = value;
    else if (key == "velocity")
      velocity = atoi(value.c_str());
  }

  if (drumName.empty()) {
    m_error.Set("pattern() requires drum parameter");
    return false;
  }
  if (grid.empty()) {
    m_error.Set("pattern() requires grid parameter");
    return false;
  }

  int trackIndex = (m_trackIndex >= 0) ? m_trackIndex : GetSelectedTrackIndex();
  Log("MAGDA Drummer: Adding pattern drum=%s grid=%s velocity=%d to track %d\n",
      drumName.c_str(), grid.c_str(), velocity, trackIndex);

  WDL_FastString errorMsg;
  bool success = MagdaActions::AddDrumPattern(
      trackIndex, drumName.c_str(), grid.c_str(), velocity, nullptr, errorMsg);

  if (!success) {
    m_error.SetFormatted(512, "AddDrumPattern failed: %s", errorMsg.Get());
  }

  return success;
}

int Interpreter::GetSelectedTrackIndex() {
  return MagdaDSLContext::Get().ResolveTargetTrack(nullptr);
}

} // namespace MagdaDrummer
