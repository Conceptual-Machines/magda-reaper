#pragma once

#include "reaper_api_wrapper.h"
#include "reaper_plugin.h"

// Project-level utilities
class Project {
public:
  // Time conversion
  static double barToTime(int bar);
  static int timeToBar(double time);
  static double barsToTime(int bars);

  // Project info
  static bool getName(char *buf, int buf_size);
  static double getLength();
  static double getTempo();
  static bool getTimeSignature(int *numerator, int *denominator);

  // Updates
  static void updateArrange();
};
