#pragma once

#include "../WDL/WDL/wdlstring.h"

// Executor module for MAGDA
// Handles execution of actions without UI dependencies
class MagdaExecutor {
public:
  // Execute action based on user input
  // Currently: creates a track with the input text as the name
  // Returns true on success, false on error
  // error_msg is populated on error
  static bool ExecuteAction(const char *input, WDL_FastString &error_msg);
};
