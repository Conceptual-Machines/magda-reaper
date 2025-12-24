#include "magda_actions.h"
#include "magda_bounce_workflow.h"
#include "magda_chat_window.h"
#include "magda_drum_mapping.h"
#include "magda_drum_mapping_window.h"
#include "magda_dsp_analyzer.h"
#include "magda_imgui_chat.h"
#include "magda_imgui_login.h"
#include "magda_imgui_mix_analysis_dialog.h"
#include "magda_imgui_plugin_window.h"
#include "magda_imgui_settings.h"
#include "magda_jsfx_editor.h"
#include "magda_param_mapping.h"
#include "magda_param_mapping_window.h"
#include "magda_plugin_scanner.h"
#include "magda_plugin_window.h"
#include "reaper_plugin.h"
// SWELL is already included by reaper_plugin.h
#include <thread>

// Plugin instance handle
HINSTANCE g_hInst = nullptr;
reaper_plugin_info_t *g_rec = nullptr;

// Global chat window instance (SWELL fallback)
static MagdaChatWindow *g_chatWindow = nullptr;
// Global ImGui chat window instance
MagdaImGuiChat *g_imguiChat = nullptr;
// Flag for using ImGui chat vs SWELL chat
static bool g_useImGuiChat = false;
// Global ImGui login window instance
MagdaImGuiLogin *g_imguiLogin = nullptr;
// Global ImGui settings window instance
MagdaImGuiSettings *g_imguiSettings = nullptr;
// Global plugin scanner instance
MagdaPluginScanner *g_pluginScanner = nullptr;
// Global plugin window instance (SWELL fallback)
MagdaPluginWindow *g_pluginWindow = nullptr;
// Global ImGui plugin window instance
MagdaImGuiPluginWindow *g_imguiPluginWindow = nullptr;
// Global ImGui mix analysis dialog instance
MagdaImGuiMixAnalysisDialog *g_imguiMixAnalysisDialog = nullptr;
// Global JSFX editor instance
MagdaJSFXEditor *g_jsfxEditor = nullptr;
// Global param mapping manager instance
MagdaParamMappingWindow *g_paramMappingWindow = nullptr;
// Global drum mapping manager instance (defined in magda_drum_mapping.cpp)
// Global drum mapping window instance (defined in
// magda_drum_mapping_window.cpp)

// Separate timer callback for processing Reaper command queue
// This runs at lower frequency and is separate from UI rendering
static void commandQueueTimerCallback() {
  // Process Reaper command queue (render, delete operations)
  MagdaBounceWorkflow::ProcessCommandQueue();

  // Process cleanup queue (legacy track deletion)
  MagdaBounceWorkflow::ProcessCleanupQueue();
}

// Timer callback for ImGui rendering
static void imguiTimerCallback() {
  if (g_imguiChat && g_imguiChat->IsVisible()) {
    g_imguiChat->Render();
  }
  // Render ImGui login window if visible
  if (g_imguiLogin && g_imguiLogin->IsVisible()) {
    g_imguiLogin->Render();
  }
  // Render ImGui settings window if visible
  if (g_imguiSettings && g_imguiSettings->IsVisible()) {
    g_imguiSettings->Render();
  }
  // Also render ImGui plugin window if visible
  if (g_imguiPluginWindow && g_imguiPluginWindow->IsVisible()) {
    g_imguiPluginWindow->Render();
  }
  // Render mix analysis dialog if visible
  if (g_imguiMixAnalysisDialog && g_imguiMixAnalysisDialog->IsVisible()) {
    g_imguiMixAnalysisDialog->Render();

    // Check if dialog was completed and process result
    if (g_imguiMixAnalysisDialog->IsCompleted()) {
      const auto &result = g_imguiMixAnalysisDialog->GetResult();

      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

      if (result.cancelled) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Mix analysis cancelled by user\n");
        }
        g_imguiMixAnalysisDialog->Reset();
        return;
      }

      // Execute workflow with track type and user query
      if (ShowConsoleMsg) {
        char msg[512];
        snprintf(msg, sizeof(msg), "MAGDA: Track type: %s, Query: %s\n",
                 result.trackType.Get(),
                 result.userQuery.GetLength() > 0 ? result.userQuery.Get()
                                                  : "(none)");
        ShowConsoleMsg(msg);
      }

      // Get bounce mode preference
      BounceMode bounceMode = MagdaBounceWorkflow::GetBounceModePreference();

      // Execute workflow
      WDL_FastString error_msg;
      bool success = MagdaBounceWorkflow::ExecuteWorkflow(
          bounceMode, result.trackType.Get(),
          result.userQuery.GetLength() > 0 ? result.userQuery.Get() : "",
          error_msg);

      if (!success) {
        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(msg, sizeof(msg),
                   "MAGDA: Mix analysis workflow failed: %s\n",
                   error_msg.Get());
          ShowConsoleMsg(msg);
        }
      } else {
        if (ShowConsoleMsg) {
          ShowConsoleMsg(
              "MAGDA: Mix analysis workflow completed successfully!\n");
        }
      }

      // Update arrange view
      void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
      if (UpdateArrange) {
        UpdateArrange();
      }

      // Reset dialog
      g_imguiMixAnalysisDialog->Reset();
    }
  }
  // Also render param mapping window if visible
  if (g_paramMappingWindow && g_paramMappingWindow->IsVisible()) {
    g_paramMappingWindow->Render();
  }
  // Render JSFX editor if visible
  if (g_jsfxEditor && g_jsfxEditor->IsVisible()) {
    g_jsfxEditor->Render();
  }
  // Also render drum mapping window if visible
  if (g_drumMappingWindow && g_drumMappingWindow->IsVisible()) {
    g_drumMappingWindow->Render();
  }
}

// Command IDs - dynamically allocated to avoid conflicts with REAPER built-ins
// These are set in the entry point via rec->Register("command_id", ...)
int g_cmdMixAnalyze = 0;    // Exported for use by other files via extern
int g_cmdMasterAnalyze = 0; // Exported for use by other files via extern

// Internal command IDs (still using high numbers to avoid conflicts)
static int g_cmdMenuID = 0;
static int g_cmdOpen = 0;
static int g_cmdLogin = 0;
static int g_cmdSettings = 0;
static int g_cmdAbout = 0;
static int g_cmdScanPlugins = 0;
static int g_cmdAnalyzeTrack = 0;
static int g_cmdTestExecute = 0;
static int g_cmdJSFXEditor = 0;

// Macros for code compatibility
#define MAGDA_MENU_CMD_ID g_cmdMenuID
#define MAGDA_CMD_OPEN g_cmdOpen
#define MAGDA_CMD_LOGIN g_cmdLogin
#define MAGDA_CMD_SETTINGS g_cmdSettings
#define MAGDA_CMD_ABOUT g_cmdAbout
#define MAGDA_CMD_SCAN_PLUGINS g_cmdScanPlugins
#define MAGDA_CMD_ANALYZE_TRACK g_cmdAnalyzeTrack
#define MAGDA_CMD_MIX_ANALYZE g_cmdMixAnalyze
#define MAGDA_CMD_MASTER_ANALYZE g_cmdMasterAnalyze
#define MAGDA_CMD_TEST_EXECUTE_ACTION g_cmdTestExecute
#define MAGDA_CMD_JSFX_EDITOR g_cmdJSFXEditor

// Helper function to perform DSP analysis and output results
static void performDSPAnalysis(int trackIndex, const char *trackName,
                               float analysisLength, bool isPostFX) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  DSPAnalysisConfig config;
  config.fftSize = 4096;
  config.analysisLength = analysisLength;
  config.analyzeFullItem = true;

  DSPAnalysisResult result = MagdaDSPAnalyzer::AnalyzeTrack(trackIndex, config);

  if (result.success) {
    WDL_FastString json;
    MagdaDSPAnalyzer::ToJSON(result, json);

    if (ShowConsoleMsg) {
      const char *fxStatus = isPostFX ? "(Post-FX)" : "(Raw - no FX)";
      char header[128];
      snprintf(header, sizeof(header), "MAGDA: Analysis complete! %s\n",
               fxStatus);
      ShowConsoleMsg(header);
      ShowConsoleMsg("--- DSP Analysis Results ---\n");

      char summary[1024];
      snprintf(summary, sizeof(summary),
               "Track: '%s' (index %d)\n"
               "Analyzed: %.2f sec, %d Hz, %d channels\n"
               "Loudness: RMS=%.1f dB, Peak=%.1f dB, LUFS≈%.1f\n"
               "Dynamics: Range=%.1f dB, Crest=%.1f dB\n"
               "Stereo: Width=%.2f, Correlation=%.2f\n"
               "Freq Bands: Sub=%.1f, Bass=%.1f, Mid=%.1f, High=%.1f dB\n"
               "Resonances detected: %zu\n",
               trackName, trackIndex, result.lengthSeconds, result.sampleRate,
               result.channels, result.loudness.rms, result.loudness.peak,
               result.loudness.lufs, result.dynamics.dynamicRange,
               result.dynamics.crestFactor, result.stereo.width,
               result.stereo.correlation, result.bands.sub, result.bands.bass,
               result.bands.mid, result.bands.brilliance,
               result.resonances.size());
      ShowConsoleMsg(summary);

      if (!result.resonances.empty()) {
        ShowConsoleMsg("Resonances:\n");
        for (size_t i = 0; i < result.resonances.size(); i++) {
          char resBuf[256];
          snprintf(resBuf, sizeof(resBuf),
                   "  %.0f Hz: +%.1f dB (Q=%.1f) - %s %s\n",
                   result.resonances[i].frequency,
                   result.resonances[i].magnitude, result.resonances[i].q,
                   result.resonances[i].severity, result.resonances[i].type);
          ShowConsoleMsg(resBuf);
        }
      }
    }

    // Store result in project state for API access
    void (*SetProjExtState)(ReaProject *, const char *, const char *,
                            const char *) =
        (void (*)(ReaProject *, const char *, const char *,
                  const char *))g_rec->GetFunc("SetProjExtState");
    if (SetProjExtState) {
      SetProjExtState(nullptr, "MAGDA_DSP", "ANALYSIS_JSON", json.Get());
      SetProjExtState(nullptr, "MAGDA_DSP", "TRACK_INDEX",
                      std::to_string(trackIndex).c_str());
      SetProjExtState(nullptr, "MAGDA_DSP", "TRACK_NAME", trackName);
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Analysis stored in project state (MAGDA_DSP)\n");
      }
    }
  } else {
    if (ShowConsoleMsg) {
      char errMsg[512];
      snprintf(errMsg, sizeof(errMsg), "MAGDA: Analysis failed: %s\n",
               result.errorMessage.Get());
      ShowConsoleMsg(errMsg);
    }
  }
}

// Action callbacks for MAGDA menu items
void magdaAction(int command_id, int flag) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  // Using if-else because command IDs are runtime values
  if (command_id == g_cmdOpen) {
    // Use ImGui chat if available, otherwise fall back to SWELL
    if (g_useImGuiChat && g_imguiChat) {
      // Only show if not already visible - don't close if already open
      if (!g_imguiChat->IsVisible()) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Opening chat interface\n");
        }
        g_imguiChat->Show();
      }
    } else {
      if (!g_chatWindow) {
        g_chatWindow = new MagdaChatWindow();
      }
      // Only show if not already visible - don't close if already open
      if (!g_chatWindow->IsVisible()) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Opening chat interface\n");
        }
        g_chatWindow->Show(false); // Show only, don't toggle
      }
    }
  } else if (command_id == g_cmdLogin) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening login dialog\n");
    }
    if (g_imguiLogin && g_imguiLogin->IsAvailable()) {
      if (!g_imguiLogin->IsVisible()) {
        g_imguiLogin->Show();
      }
    } else {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Login requires ReaImGui extension\n");
      }
    }
  } else if (command_id == g_cmdSettings) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening settings dialog\n");
    }
    if (g_imguiSettings && g_imguiSettings->IsAvailable()) {
      if (!g_imguiSettings->IsVisible()) {
        g_imguiSettings->Show();
      }
    } else {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Settings requires ReaImGui extension\n");
      }
    }
  } else if (command_id == g_cmdAbout) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: About - TODO: Show about dialog\n");
    }
    // TODO: Show about dialog
  } else if (command_id == g_cmdScanPlugins) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening plugin alias window\n");
    }
    // Use ImGui plugin window if available
    if (g_imguiPluginWindow && g_imguiPluginWindow->IsAvailable()) {
      g_imguiPluginWindow->Toggle();
    } else {
      // Fallback to SWELL window
      if (!g_pluginWindow) {
        g_pluginWindow = new MagdaPluginWindow();
      }
      g_pluginWindow->Show();
    }
  } else if (command_id == g_cmdAnalyzeTrack) {
    // Analyze the first selected track's audio (non-blocking)
    // Queue the work to run in background thread to avoid blocking UI
    {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Queuing track analysis (non-blocking)...\n");
      }

      // Run analysis in background thread to avoid blocking
      std::thread([]() {
        void (*ShowConsoleMsg)(const char *msg) =
            (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

        // Get required REAPER functions (safe to call from background thread
        // for read-only ops)
        int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
        MediaTrack *(*GetTrack)(ReaProject *, int) =
            (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
        int (*IsTrackSelected)(MediaTrack *) =
            (int (*)(MediaTrack *))g_rec->GetFunc("IsTrackSelected");
        bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *,
                                            bool) =
            (bool (*)(MediaTrack *, const char *, char *, bool))g_rec->GetFunc(
                "GetSetMediaTrackInfo_String");
        void (*GetSet_LoopTimeRange2)(ReaProject *, bool, bool, double *,
                                      double *, bool) =
            (void (*)(ReaProject *, bool, bool, double *, double *,
                      bool))g_rec->GetFunc("GetSet_LoopTimeRange2");
        double (*GetProjectLength)(ReaProject *) =
            (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");
        int (*CountTrackMediaItems)(MediaTrack *) =
            (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");

        if (!GetNumTracks || !GetTrack || !IsTrackSelected) {
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA: Required REAPER functions not available\n");
          }
          return;
        }

        int numTracks = GetNumTracks();
        int selectedTrackIndex = -1;
        MediaTrack *selectedTrack = nullptr;
        char trackName[256] = "Track";

        // Find selected track
        for (int i = 0; i < numTracks; i++) {
          MediaTrack *track = GetTrack(nullptr, i);
          if (track && IsTrackSelected(track)) {
            selectedTrackIndex = i;
            selectedTrack = track;
            if (GetSetMediaTrackInfo_String) {
              GetSetMediaTrackInfo_String(track, "P_NAME", trackName, false);
            }
            break;
          }
        }

        if (selectedTrackIndex < 0) {
          if (ShowConsoleMsg) {
            ShowConsoleMsg(
                "MAGDA: No track selected. Please select a track first.\n");
          }
          return;
        }

        if (ShowConsoleMsg) {
          char msg[512];
          snprintf(msg, sizeof(msg), "MAGDA: Analyzing track %d ('%s')...\n",
                   selectedTrackIndex, trackName);
          ShowConsoleMsg(msg);
        }

        // Check if we have a time selection
        double timeSelStart = 0, timeSelEnd = 0;
        bool hasTimeSelection = false;
        if (GetSet_LoopTimeRange2) {
          GetSet_LoopTimeRange2(nullptr, false, false, &timeSelStart,
                                &timeSelEnd, false);
          hasTimeSelection = (timeSelEnd - timeSelStart) > 0.1;
        }

        // Determine analysis length
        double analysisLength = 30.0;
        if (!hasTimeSelection) {
          double projLen = GetProjectLength ? GetProjectLength(nullptr) : 60.0;
          if (projLen < analysisLength)
            analysisLength = projLen;
          if (analysisLength < 1.0)
            analysisLength = 1.0;
          timeSelEnd = timeSelStart + analysisLength;
        } else {
          analysisLength = timeSelEnd - timeSelStart;
        }

        if (ShowConsoleMsg) {
          char msg[256];
          snprintf(msg, sizeof(msg),
                   "MAGDA: Analysis range: %.2f - %.2f sec (%.1f sec)\n",
                   timeSelStart, timeSelEnd, analysisLength);
          ShowConsoleMsg(msg);
        }

        // Check if track has audio items
        bool hasAudioItems = false;
        if (CountTrackMediaItems && selectedTrack) {
          hasAudioItems = CountTrackMediaItems(selectedTrack) > 0;
        }

        if (!hasAudioItems) {
          if (ShowConsoleMsg) {
            ShowConsoleMsg("MAGDA: Track has no media items to analyze.\n");
            ShowConsoleMsg(
                "MAGDA: For VSTi tracks, please bounce to audio first.\n");
          }
          return;
        }

        // Perform DSP analysis directly on the track's audio
        // This reads the source audio (pre-FX for audio tracks)
        performDSPAnalysis(selectedTrackIndex, trackName, (float)analysisLength,
                           false);
      }).detach();
    }
  } else if (command_id == g_cmdMixAnalyze) {
    // Mix analysis: open chat with @mix: prefilled
    // User can then type track type and query
    {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Opening chat for mix analysis...\n");
      }

      if (g_imguiChat && g_imguiChat->IsAvailable()) {
        // Try to guess track type from selected track name
        std::string prefill = "@mix:";

        // Get selected track name for smart suggestion
        MediaTrack *(*GetSelectedTrack)(ReaProject *, int) =
            (MediaTrack * (*)(ReaProject *, int))
                g_rec->GetFunc("GetSelectedTrack");
        bool (*GetTrackName)(MediaTrack *, char *, int) =
            (bool (*)(MediaTrack *, char *, int))g_rec->GetFunc("GetTrackName");

        if (GetSelectedTrack && GetTrackName) {
          MediaTrack *track = GetSelectedTrack(nullptr, 0);
          if (track) {
            char trackName[256] = {0};
            if (GetTrackName(track, trackName, sizeof(trackName)) &&
                trackName[0]) {
              // Simple keyword detection (could be improved later)
              std::string lowerName = trackName;
              for (auto &c : lowerName)
                c = tolower(c);

              if (lowerName.find("drum") != std::string::npos ||
                  lowerName.find("kick") != std::string::npos ||
                  lowerName.find("snare") != std::string::npos ||
                  lowerName.find("hat") != std::string::npos) {
                prefill += "drums ";
              } else if (lowerName.find("bass") != std::string::npos ||
                         lowerName.find("808") != std::string::npos) {
                prefill += "bass ";
              } else if (lowerName.find("synth") != std::string::npos ||
                         lowerName.find("pad") != std::string::npos ||
                         lowerName.find("lead") != std::string::npos) {
                prefill += "synth ";
              } else if (lowerName.find("vocal") != std::string::npos ||
                         lowerName.find("vox") != std::string::npos) {
                prefill += "vocals ";
              } else if (lowerName.find("guitar") != std::string::npos ||
                         lowerName.find("gtr") != std::string::npos) {
                prefill += "guitar ";
              } else if (lowerName.find("piano") != std::string::npos ||
                         lowerName.find("keys") != std::string::npos) {
                prefill += "piano ";
              }
              // If no match, just leave @mix: for user to type
            }
          }
        }

        g_imguiChat->ShowWithInput(prefill.c_str());
      } else {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Chat not available (ReaImGui required)\n");
        }
      }
    }
  } else if (command_id == g_cmdMasterAnalyze) {
    // Master analysis: open chat with @master: prefilled
    {
      void (*ShowConsoleMsg)(const char *msg) =
          (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Opening chat for master analysis...\n");
      }

      if (g_imguiChat && g_imguiChat->IsAvailable()) {
        g_imguiChat->ShowWithInput("@master:");
      } else {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Chat not available (ReaImGui required)\n");
        }
      }
    }
  } else if (command_id == g_cmdJSFXEditor) {
    // Open JSFX Editor
    void (*ShowConsoleMsg)(const char *msg) =
        (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening JSFX Editor...\n");
    }
    if (g_jsfxEditor && g_jsfxEditor->IsAvailable()) {
      g_jsfxEditor->Show();
    } else if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: JSFX Editor not available (ReaImGui required)\n");
    }
  } else if (command_id == g_cmdTestExecute) {
    // Headless/test mode: Execute MAGDA action from JSON stored in project
    // state This allows Lua scripts to execute MAGDA commands by setting
    // project state
    {
      // Get JSON action from project state (stored via SetProjExtState)
      void *(*GetProjExtState)(ReaProject *, const char *, const char *, char *,
                               int) =
          (void *(*)(ReaProject *, const char *, const char *, char *,
                     int))g_rec->GetFunc("GetProjExtState");

      if (GetProjExtState) {
        char action_json[4096] = {0};
        GetProjExtState(nullptr, "MAGDA_TEST", "ACTION_JSON", action_json,
                        sizeof(action_json));

        if (action_json[0]) {
          WDL_FastString result, error;
          if (MagdaActions::ExecuteActions(action_json, result, error)) {
            // Store result in project state for Lua to read
            void (*SetProjExtState)(ReaProject *, const char *, const char *,
                                    const char *) =
                (void (*)(ReaProject *, const char *, const char *,
                          const char *))g_rec->GetFunc("SetProjExtState");
            if (SetProjExtState) {
              SetProjExtState(nullptr, "MAGDA_TEST", "RESULT", result.Get());
              SetProjExtState(nullptr, "MAGDA_TEST", "ERROR", "");
            }
            if (ShowConsoleMsg) {
              ShowConsoleMsg("MAGDA: Test action executed successfully\n");
            }
          } else {
            // Store error
            void (*SetProjExtState)(ReaProject *, const char *, const char *,
                                    const char *) =
                (void (*)(ReaProject *, const char *, const char *,
                          const char *))g_rec->GetFunc("SetProjExtState");
            if (SetProjExtState) {
              SetProjExtState(nullptr, "MAGDA_TEST", "RESULT", "");
              SetProjExtState(nullptr, "MAGDA_TEST", "ERROR", error.Get());
            }
            if (ShowConsoleMsg) {
              char msg[512];
              snprintf(msg, sizeof(msg), "MAGDA: Test action failed: %s\n",
                       error.Get());
              ShowConsoleMsg(msg);
            }
          }
        }
      }
    }
  } else {
    if (ShowConsoleMsg) {
      char msg[128];
      snprintf(msg, sizeof(msg), "MAGDA: Unknown command ID: %d\n", command_id);
      ShowConsoleMsg(msg);
    }
  }
}

// Static function for hookcommand2 - must be defined before entry point
static bool hookcmd_func(KbdSectionInfo *sec, int command, int val, int val2,
                         int relmode, HWND hwnd) {
  (void)sec;
  (void)val;
  (void)val2;
  (void)relmode;
  (void)hwnd; // Suppress unused warnings

  // Only handle if IDs are allocated (non-zero) and command matches
  if ((g_cmdMenuID != 0 && command == g_cmdMenuID) ||
      (g_cmdOpen != 0 && command == g_cmdOpen) ||
      (g_cmdLogin != 0 && command == g_cmdLogin) ||
      (g_cmdSettings != 0 && command == g_cmdSettings) ||
      (g_cmdAbout != 0 && command == g_cmdAbout) ||
      (g_cmdScanPlugins != 0 && command == g_cmdScanPlugins) ||
      (g_cmdAnalyzeTrack != 0 && command == g_cmdAnalyzeTrack) ||
      (g_cmdMixAnalyze != 0 && command == g_cmdMixAnalyze) ||
      (g_cmdMasterAnalyze != 0 && command == g_cmdMasterAnalyze) ||
      (g_cmdJSFXEditor != 0 && command == g_cmdJSFXEditor) ||
      (g_cmdTestExecute != 0 && command == g_cmdTestExecute)) {
    magdaAction(command, 0);
    return true; // handled
  }
  return false; // not handled
}

// Menu hook to populate MAGDA menu
void menuHook(const char *menuidstr, void *menu, int flag) {
  if (!g_rec)
    return;

  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  // Handle "Main extensions" menu - this is where we add our menu item
  if (menuidstr && strcmp(menuidstr, "Main extensions") == 0 && flag == 0) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: menuHook - Main extensions, flag=0\n");
    }

    HMENU hMenu = (HMENU)menu;
    if (!hMenu) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ERROR - hMenu is NULL!\n");
      }
      return;
    }

    // Now that we're linking against SWELL stub, function pointers should be
    // initialized Add menu item to "Main extensions" - following SWS pattern
    // exactly
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Adding menu item to Main extensions\n");
    }

    // Get menu item count right before inserting to ensure we're at the end
    // Other extensions might add items during the same hook call, so we check
    // right before inserting
    int itemCount = GetMenuItemCount(hMenu);
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA: Initial menu count: %d items\n",
               itemCount);
      ShowConsoleMsg(msg);
    }

    // Create MENUITEMINFO structure - following SWS pattern exactly
    MENUITEMINFO mi = {
        sizeof(MENUITEMINFO),
    };
    mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    mi.fType = MFT_STRING;
    mi.fState = MFS_UNCHECKED;
    mi.dwTypeData = (char *)"MAGDA";
    mi.wID = MAGDA_MENU_CMD_ID;

    // Re-check count right before inserting (in case other extensions added
    // items)
    itemCount = GetMenuItemCount(hMenu);

    // Create a submenu for MAGDA
    HMENU hSubMenu = CreatePopupMenu();
    if (!hSubMenu) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ERROR - Failed to create submenu!\n");
      }
      return;
    }

    // Add items to the submenu
    MENUITEMINFO subMi = {
        sizeof(MENUITEMINFO),
    };
    subMi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    subMi.fType = MFT_STRING;
    subMi.fState = MFS_UNCHECKED;

    // "MAGDA Chat" item (main chat window)
    subMi.dwTypeData = (char *)"MAGDA Chat";
    subMi.wID = MAGDA_CMD_OPEN;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "JSFX Editor" item
    subMi.dwTypeData = (char *)"JSFX Editor";
    subMi.wID = MAGDA_CMD_JSFX_EDITOR;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "Plugins" item (opens plugin alias window)
    subMi.dwTypeData = (char *)"Plugins...";
    subMi.wID = MAGDA_CMD_SCAN_PLUGINS;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // Separator
    subMi.fMask = MIIM_TYPE;
    subMi.fType = MFT_SEPARATOR;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "Login" item
    subMi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    subMi.fType = MFT_STRING;
    subMi.fState = MFS_UNCHECKED;
    subMi.dwTypeData = (char *)"Login...";
    subMi.wID = MAGDA_CMD_LOGIN;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "Settings" item
    subMi.dwTypeData = (char *)"Settings...";
    subMi.wID = MAGDA_CMD_SETTINGS;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // Separator
    subMi.fMask = MIIM_TYPE;
    subMi.fType = MFT_SEPARATOR;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "About" item
    subMi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    subMi.fType = MFT_STRING;
    subMi.fState = MFS_UNCHECKED;
    subMi.dwTypeData = (char *)"About MAGDA...";
    subMi.wID = MAGDA_CMD_ABOUT;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // Note: Mix Analysis removed from menu - use @mix: in chat instead
    // The action is still registered so it can be triggered via Actions list

    // Now add the MAGDA menu item with submenu to Main extensions
    mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
    mi.hSubMenu = hSubMenu;
    mi.dwTypeData = (char *)"MAGDA";
    mi.wID = MAGDA_MENU_CMD_ID;

    // Insert the menu item with submenu at the end
    InsertMenuItem(hMenu, itemCount, true, &mi);

    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Menu with submenu added successfully!\n");
    }
  }
  // We're not using a custom menu anymore - just adding to Main extensions
}

// Reaper extension entry point
extern "C" {
REAPER_PLUGIN_DLL_EXPORT int
REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                         reaper_plugin_info_t *rec) {
  if (!rec) {
    // Extension is being unloaded
    if (g_imguiPluginWindow) {
      delete g_imguiPluginWindow;
      g_imguiPluginWindow = nullptr;
    }
    if (g_paramMappingWindow) {
      delete g_paramMappingWindow;
      g_paramMappingWindow = nullptr;
    }
    if (g_paramMappingManager) {
      delete g_paramMappingManager;
      g_paramMappingManager = nullptr;
    }
    if (g_drumMappingWindow) {
      delete g_drumMappingWindow;
      g_drumMappingWindow = nullptr;
    }
    if (g_drumMappingManager) {
      delete g_drumMappingManager;
      g_drumMappingManager = nullptr;
    }
    if (g_imguiChat) {
      delete g_imguiChat;
      g_imguiChat = nullptr;
    }
    if (g_imguiLogin) {
      delete g_imguiLogin;
      g_imguiLogin = nullptr;
    }
    if (g_imguiSettings) {
      delete g_imguiSettings;
      g_imguiSettings = nullptr;
    }
    if (g_imguiMixAnalysisDialog) {
      delete g_imguiMixAnalysisDialog;
      g_imguiMixAnalysisDialog = nullptr;
    }
    if (g_chatWindow) {
      delete g_chatWindow;
      g_chatWindow = nullptr;
    }
    if (g_pluginScanner) {
      delete g_pluginScanner;
      g_pluginScanner = nullptr;
    }
    return 0;
  }

  if (rec->caller_version != REAPER_PLUGIN_VERSION) {
    // Version mismatch
    return 0;
  }

  // Store plugin handle and API
  g_hInst = hInstance;
  g_rec = rec;

  // Get console message function for debugging
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))rec->GetFunc("ShowConsoleMsg");

  // Always try to show a message - even if ShowConsoleMsg is NULL, we'll
  // continue
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Extension entry point called\n");
    ShowConsoleMsg("MAGDA: Testing console output...\n");
  }

  // Allocate unique command IDs dynamically to avoid conflicts with REAPER
  // built-ins
  g_cmdMenuID = rec->Register("command_id", (void *)"MAGDA_Menu");
  g_cmdOpen = rec->Register("command_id", (void *)"MAGDA_Open");
  g_cmdLogin = rec->Register("command_id", (void *)"MAGDA_Login");
  g_cmdSettings = rec->Register("command_id", (void *)"MAGDA_Settings");
  g_cmdAbout = rec->Register("command_id", (void *)"MAGDA_About");
  g_cmdScanPlugins = rec->Register("command_id", (void *)"MAGDA_ScanPlugins");
  g_cmdAnalyzeTrack = rec->Register("command_id", (void *)"MAGDA_AnalyzeTrack");
  g_cmdMixAnalyze = rec->Register("command_id", (void *)"MAGDA_MixAnalyze");
  g_cmdMasterAnalyze =
      rec->Register("command_id", (void *)"MAGDA_MasterAnalyze");
  g_cmdJSFXEditor = rec->Register("command_id", (void *)"MAGDA_JSFXEditor");
  g_cmdTestExecute = rec->Register("command_id", (void *)"MAGDA_TestExecute");

  if (ShowConsoleMsg) {
    char msg[512];
    snprintf(msg, sizeof(msg),
             "MAGDA: Allocated command IDs: Open=%d, Login=%d, Settings=%d, "
             "About=%d, Plugins=%d, Analyze=%d, MixAnalyze=%d, "
             "MasterAnalyze=%d, Test=%d\n",
             g_cmdOpen, g_cmdLogin, g_cmdSettings, g_cmdAbout, g_cmdScanPlugins,
             g_cmdAnalyzeTrack, g_cmdMixAnalyze, g_cmdMasterAnalyze,
             g_cmdTestExecute);
    ShowConsoleMsg(msg);
  }

  // Register actions for all menu items (using dynamically allocated IDs)
  gaccel_register_t gaccel_open = {{0, 0, (unsigned short)g_cmdOpen},
                                   "MAGDA: Open MAGDA"};
  gaccel_register_t gaccel_login = {{0, 0, (unsigned short)g_cmdLogin},
                                    "MAGDA: Login"};
  gaccel_register_t gaccel_settings = {{0, 0, (unsigned short)g_cmdSettings},
                                       "MAGDA: Settings"};
  gaccel_register_t gaccel_about = {{0, 0, (unsigned short)g_cmdAbout},
                                    "MAGDA: About"};
  gaccel_register_t gaccel_scan_plugins = {
      {0, 0, (unsigned short)g_cmdScanPlugins}, "MAGDA: Plugins"};
  gaccel_register_t gaccel_analyze_track = {
      {0, 0, (unsigned short)g_cmdAnalyzeTrack},
      "MAGDA: Analyze Selected Track"};
  gaccel_register_t gaccel_mix_analyze = {
      {0, 0, (unsigned short)g_cmdMixAnalyze}, "MAGDA: Mix Analysis"};
  gaccel_register_t gaccel_master_analyze = {
      {0, 0, (unsigned short)g_cmdMasterAnalyze}, "MAGDA: Master Analysis"};
  gaccel_register_t gaccel_jsfx_editor = {
      {0, 0, (unsigned short)g_cmdJSFXEditor}, "MAGDA: JSFX Editor"};
  gaccel_register_t gaccel_test_execute = {
      {0, 0, (unsigned short)g_cmdTestExecute}, "MAGDA: Test Execute Action"};

  if (rec->Register("gaccel", &gaccel_open)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Open MAGDA' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_login)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Login' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_settings)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Settings' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_about)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'About' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_scan_plugins)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Plugins' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_analyze_track)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Analyze Selected Track' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_mix_analyze)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Mix Analysis' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_master_analyze)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Master Analysis' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_jsfx_editor)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'JSFX Editor' action\n");
    }
  }
  if (rec->Register("gaccel", &gaccel_test_execute)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg(
          "MAGDA: Registered 'Test Execute Action' (for headless testing)\n");
    }
  }

  // Initialize plugin scanner
  g_pluginScanner = new MagdaPluginScanner();
  // Try to load from cache first
  if (!g_pluginScanner->LoadFromCache() || !g_pluginScanner->IsCacheValid()) {
    // Cache not valid, scan plugins
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Scanning plugins on startup...\n");
    }
    int count = g_pluginScanner->ScanPlugins();
    g_pluginScanner->SaveToCache();
    if (ShowConsoleMsg) {
      char msg[256];
      snprintf(msg, sizeof(msg), "MAGDA: Scanned %d plugins on startup\n",
               count);
      ShowConsoleMsg(msg);
    }
  } else {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Loaded plugins from cache\n");
    }
  }

  // Try to initialize ImGui chat (requires ReaImGui extension)
  g_imguiChat = new MagdaImGuiChat();
  if (g_imguiChat->Initialize(rec)) {
    g_useImGuiChat = true;
    g_imguiChat->SetPluginScanner(g_pluginScanner);
    // Register timer for ImGui rendering (~30 fps)
    rec->Register("timer", (void *)imguiTimerCallback);
    // Register separate timer for command queue processing (lower frequency,
    // separate from UI)
    rec->Register("timer", (void *)commandQueueTimerCallback);
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: ImGui chat initialized (ReaImGui available)\n");
    }

    // Initialize ImGui login window
    g_imguiLogin = new MagdaImGuiLogin();
    if (g_imguiLogin->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui login initialized\n");
      }
    } else {
      delete g_imguiLogin;
      g_imguiLogin = nullptr;
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui login requires ReaImGui extension\n");
      }
    }

    // Initialize ImGui settings window
    g_imguiSettings = new MagdaImGuiSettings();
    if (g_imguiSettings->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui settings initialized\n");
      }
    } else {
      delete g_imguiSettings;
      g_imguiSettings = nullptr;
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui settings requires ReaImGui extension\n");
      }
    }

    // Initialize drum mapping window (also uses ReaImGui)
    g_drumMappingManager = new DrumMappingManager();
    g_drumMappingWindow = new MagdaDrumMappingWindow();
    if (g_drumMappingWindow->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Drum mapping window initialized\n");
      }
    }

    // Initialize ImGui plugin window
    g_imguiPluginWindow = new MagdaImGuiPluginWindow();
    if (g_imguiPluginWindow->Initialize(rec)) {
      g_imguiPluginWindow->SetPluginScanner(g_pluginScanner);
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui plugin window initialized\n");
      }
    }

    // Initialize param mapping
    g_paramMappingManager = new ParamMappingManager();
    g_paramMappingWindow = new MagdaParamMappingWindow();
    if (g_paramMappingWindow->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Param mapping window initialized\n");
      }
    }

    // Initialize ImGui mix analysis dialog
    g_imguiMixAnalysisDialog = new MagdaImGuiMixAnalysisDialog();
    if (g_imguiMixAnalysisDialog->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: ImGui mix analysis dialog initialized\n");
      }
    } else {
      delete g_imguiMixAnalysisDialog;
      g_imguiMixAnalysisDialog = nullptr;
    }

    // Initialize JSFX Editor
    g_jsfxEditor = new MagdaJSFXEditor();
    if (g_jsfxEditor->Initialize(rec)) {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: JSFX Editor initialized\n");
      }
    } else {
      delete g_jsfxEditor;
      g_jsfxEditor = nullptr;
    }
  } else {
    // ReaImGui not available, fall back to SWELL chat
    delete g_imguiChat;
    g_imguiChat = nullptr;
    g_useImGuiChat = false;
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: ReaImGui not available, using SWELL chat\n");
    }
  }

  // Register command hook to handle the actions
  // hookcommand2 signature: bool onAction(KbdSectionInfo *sec, int command, int
  // val, int val2, int relmode, HWND hwnd) Use the static function defined
  // above instead of lambda for compatibility
  if (rec->Register("hookcommand2", (void *)hookcmd_func)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered hookcommand2\n");
    }
  }

  // Call AddExtensionsMainMenu() - it takes no parameters and ensures
  // Extensions menu exists
  typedef bool (*AddExtensionsMainMenuFunc)();
  AddExtensionsMainMenuFunc AddExtensionsMainMenu =
      (AddExtensionsMainMenuFunc)rec->GetFunc("AddExtensionsMainMenu");

  if (AddExtensionsMainMenu) {
    AddExtensionsMainMenu(); // Call with no parameters
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Called AddExtensionsMainMenu()\n");
    }
  }

  // Don't use AddCustomizableMenu - we'll add the menu item directly to
  // Extensions menu via hookcustommenu instead

  // Register menu hook to populate the MAGDA menu
  if (rec->Register("hookcustommenu", (void *)menuHook)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered hookcustommenu\n");
    }
  } else {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: ERROR - Failed to register hookcustommenu!\n");
    }
  }

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA Reaper Extension loaded successfully!\n");
  }

  // Extension loaded successfully
  return 1;
}
}
