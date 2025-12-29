#ifndef MAGDA_DRUMMER_INTERPRETER_H
#define MAGDA_DRUMMER_INTERPRETER_H

#include "../WDL/WDL/wdlstring.h"

namespace MagdaDrummer {

// ============================================================================
// Drummer DSL Interpreter
// ============================================================================
// Executes Drummer DSL (pattern) by calling existing AddDrumPattern

class Interpreter {
public:
  Interpreter();
  ~Interpreter();

  // Execute Drummer DSL code (pattern(...) calls)
  bool Execute(const char *dsl_code);

  // Get error message
  const char *GetError() const { return m_error.Get(); }

  // Set target track index (uses selected track if -1)
  void SetTargetTrack(int track_index) { m_trackIndex = track_index; }

private:
  bool ExecutePattern(const char *params);
  int GetSelectedTrackIndex();

  WDL_FastString m_error;
  int m_trackIndex;
};

} // namespace MagdaDrummer

#endif // MAGDA_DRUMMER_INTERPRETER_H
