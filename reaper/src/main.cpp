#include "reaper_plugin.h"

// Plugin instance handle
HINSTANCE g_hInst = nullptr;

// Command ID for our test action
int g_command_id = 0;

// Test action callback
bool testAction(int command) {
    // Simple test - show a message
    // For now, just return true to indicate success
    return true;
}

// Reaper extension entry point
extern "C" {
    REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
        REAPER_PLUGIN_HINSTANCE hInstance,
        reaper_plugin_info_t *rec)
    {
        if (!rec) {
            // Extension is being unloaded
            return 0;
        }

        if (rec->caller_version != REAPER_PLUGIN_VERSION) {
            // Version mismatch
            return 0;
        }

        // Store plugin handle
        g_hInst = hInstance;

        // Register a simple action so the extension appears in Reaper
        gaccel_register_t accel = {
            { { 0, 0, 0 }, "MAGDA_REAPER_TEST", testAction },
            "MAGDA: Test Action"
        };
        
        g_command_id = rec->Register("gaccel", &accel);
        
        // Extension loaded successfully
        return 1;
    }
}

