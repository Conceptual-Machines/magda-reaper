#include "magda_jsfx_interpreter.h"
#include "magda_actions.h"
#include "reaper_plugin.h"
#include <cstring>
#include <string>

extern reaper_plugin_info_t* g_rec;

namespace MagdaJSFX {

static void Log(const char* fmt, ...) {
    if (!g_rec) return;
    void (*ShowConsoleMsg)(const char*) = (void (*)(const char*))g_rec->GetFunc("ShowConsoleMsg");
    if (!ShowConsoleMsg) return;

    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    ShowConsoleMsg(msg);
}

Interpreter::Interpreter() : m_trackIndex(-1) {}
Interpreter::~Interpreter() {}

bool Interpreter::Execute(const char* jsfx_code, const char* effect_name) {
    if (!jsfx_code || !*jsfx_code) {
        m_error.Set("No JSFX code provided");
        return false;
    }

    // Try to extract effect name from desc: line if not provided
    std::string name = effect_name ? effect_name : "";
    if (name.empty()) {
        const char* descLine = strstr(jsfx_code, "desc:");
        if (descLine) {
            descLine += 5; // Skip "desc:"
            while (*descLine == ' ') descLine++;
            const char* end = strchr(descLine, '\n');
            if (end) {
                name = std::string(descLine, end - descLine);
                // Trim trailing whitespace
                while (!name.empty() && (name.back() == ' ' || name.back() == '\r')) {
                    name.pop_back();
                }
            }
        }
        if (name.empty()) {
            name = "magda_generated";
        }
    }

    Log("MAGDA JSFX: Saving effect '%s' (%d bytes)\n", name.c_str(), (int)strlen(jsfx_code));

    WDL_FastString errorMsg;
    bool success = MagdaActions::SaveAndApplyJSFX(jsfx_code, name.c_str(), m_trackIndex, errorMsg);

    if (!success) {
        m_error.SetFormatted(512, "SaveAndApplyJSFX failed: %s", errorMsg.Get());
    }

    return success;
}

} // namespace MagdaJSFX
