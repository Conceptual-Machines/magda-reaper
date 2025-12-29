#pragma once

#include "reaper_plugin.h"

// Plugin instance handle
extern HINSTANCE g_hInst;

// Command IDs (will be registered with Reaper)
extern int command_id;

// Hook functions
extern void hook_command(int command, int flag);
extern void hook_command2(int command, int flag);
