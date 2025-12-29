#ifndef MAGDA_JSFX_INTERPRETER_H
#define MAGDA_JSFX_INTERPRETER_H

#include "../WDL/WDL/wdlstring.h"

namespace MagdaJSFX {

// ============================================================================
// JSFX Interpreter
// ============================================================================
// Saves JSFX code to file and optionally applies to track

class Interpreter {
public:
  Interpreter();
  ~Interpreter();

  // Execute JSFX code (saves to file, optionally adds to track)
  // The code should be raw JSFX, not DSL
  bool Execute(const char *jsfx_code, const char *effect_name = nullptr);

  // Get error message
  const char *GetError() const { return m_error.Get(); }

  // Set target track index (-1 = don't add to track, just save)
  void SetTargetTrack(int track_index) { m_trackIndex = track_index; }

private:
  WDL_FastString m_error;
  int m_trackIndex;
};

} // namespace MagdaJSFX

#endif // MAGDA_JSFX_INTERPRETER_H
