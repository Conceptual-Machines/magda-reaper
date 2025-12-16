#include "magda_actions.h"
#include "magda_chat_window.h"
#include "magda_dsp_analyzer.h"
#include "magda_imgui_chat.h"
#include "magda_login_window.h"
#include "magda_plugin_scanner.h"
#include "magda_plugin_window.h"
#include "magda_settings_window.h"
#include "reaper_plugin.h"
// SWELL is already included by reaper_plugin.h

// Plugin instance handle
HINSTANCE g_hInst = nullptr;
reaper_plugin_info_t *g_rec = nullptr;

// Global chat window instance (SWELL fallback)
static MagdaChatWindow *g_chatWindow = nullptr;
// Global ImGui chat window instance
MagdaImGuiChat *g_imguiChat = nullptr;
// Flag for using ImGui chat vs SWELL chat
static bool g_useImGuiChat = false;
// Global login window instance
static MagdaLoginWindow *g_loginWindow = nullptr;
// Global settings window instance
static MagdaSettingsWindow *g_settingsWindow = nullptr;
// Global plugin scanner instance
MagdaPluginScanner *g_pluginScanner = nullptr;
// Global plugin window instance
MagdaPluginWindow *g_pluginWindow = nullptr;

// Timer callback for ImGui rendering
static void imguiTimerCallback() {
  if (g_imguiChat && g_imguiChat->IsVisible()) {
    g_imguiChat->Render();
  }
}

// Command IDs for MAGDA menu items
#define MAGDA_MENU_CMD_ID 1000
#define MAGDA_CMD_OPEN 1001
#define MAGDA_CMD_LOGIN 1002
#define MAGDA_CMD_SETTINGS 1003
#define MAGDA_CMD_ABOUT 1004
#define MAGDA_CMD_SCAN_PLUGINS 1005
#define MAGDA_CMD_ANALYZE_TRACK 1006
// Test/headless command IDs
#define MAGDA_CMD_TEST_EXECUTE_ACTION 2000

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

  switch (command_id) {
  case MAGDA_CMD_OPEN:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening chat interface\n");
    }
    // Use ImGui chat if available, otherwise fall back to SWELL
    if (g_useImGuiChat && g_imguiChat) {
      g_imguiChat->Toggle();
    } else {
      if (!g_chatWindow) {
        g_chatWindow = new MagdaChatWindow();
      }
      g_chatWindow->Show(true); // Toggle - show if hidden, hide if shown
    }
    break;
  case MAGDA_CMD_LOGIN:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening login dialog\n");
    }
    if (!g_loginWindow) {
      g_loginWindow = new MagdaLoginWindow();
    }
    g_loginWindow->Show();
    break;
  case MAGDA_CMD_SETTINGS:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening settings dialog\n");
    }
    if (!g_settingsWindow) {
      g_settingsWindow = new MagdaSettingsWindow();
    }
    g_settingsWindow->Show();
    break;
  case MAGDA_CMD_ABOUT:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: About - TODO: Show about dialog\n");
    }
    // TODO: Show about dialog
    break;
  case MAGDA_CMD_SCAN_PLUGINS:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening plugin alias window\n");
    }
    if (!g_pluginWindow) {
      g_pluginWindow = new MagdaPluginWindow();
    }
    g_pluginWindow->Show();
    break;
  case MAGDA_CMD_ANALYZE_TRACK:
    // Analyze the first selected track's audio (with FX applied)
    // Custom render workflow - no built-in REAPER stem actions
    {
      if (ShowConsoleMsg) {
        ShowConsoleMsg("MAGDA: Analyzing selected track...\n");
      }

      // Get required REAPER functions
      int (*GetNumTracks)() = (int (*)())g_rec->GetFunc("GetNumTracks");
      MediaTrack *(*GetTrack)(ReaProject *, int) =
          (MediaTrack * (*)(ReaProject *, int)) g_rec->GetFunc("GetTrack");
      int (*IsTrackSelected)(MediaTrack *) =
          (int (*)(MediaTrack *))g_rec->GetFunc("IsTrackSelected");
      void (*SetTrackSelected)(MediaTrack *, bool) =
          (void (*)(MediaTrack *, bool))g_rec->GetFunc("SetTrackSelected");
      void (*SetOnlyTrackSelected)(MediaTrack *) =
          (void (*)(MediaTrack *))g_rec->GetFunc("SetOnlyTrackSelected");
      void (*Main_OnCommand)(int, int) =
          (void (*)(int, int))g_rec->GetFunc("Main_OnCommand");
      void (*GetSet_LoopTimeRange2)(ReaProject *, bool, bool, double *,
                                    double *, bool) =
          (void (*)(ReaProject *, bool, bool, double *, double *,
                    bool))g_rec->GetFunc("GetSet_LoopTimeRange2");
      double (*GetProjectLength)(ReaProject *) =
          (double (*)(ReaProject *))g_rec->GetFunc("GetProjectLength");
      void (*DeleteTrack)(MediaTrack *) =
          (void (*)(MediaTrack *))g_rec->GetFunc("DeleteTrack");
      bool (*GetSetMediaTrackInfo_String)(MediaTrack *, const char *, char *,
                                          bool) =
          (bool (*)(MediaTrack *, const char *, char *, bool))g_rec->GetFunc(
              "GetSetMediaTrackInfo_String");
      void *(*GetSetMediaTrackInfo)(MediaTrack *, const char *, void *) =
          (void *(*)(MediaTrack *, const char *, void *))g_rec->GetFunc(
              "GetSetMediaTrackInfo");
      void (*InsertTrackInProject)(ReaProject *, int, int) = (void (*)(
          ReaProject *, int, int))g_rec->GetFunc("InsertTrackInProject");
      double (*GetMediaTrackInfo_Value)(MediaTrack *, const char *) =
          (double (*)(MediaTrack *, const char *))g_rec->GetFunc(
              "GetMediaTrackInfo_Value");

      if (!GetNumTracks || !GetTrack || !IsTrackSelected) {
        if (ShowConsoleMsg) {
          ShowConsoleMsg("MAGDA: Required REAPER functions not available\n");
        }
        break;
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
        break;
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
        GetSet_LoopTimeRange2(nullptr, false, false, &timeSelStart, &timeSelEnd,
                              false);
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

      // For now, analyze raw audio directly from the track
      // This works for audio tracks but not VSTi tracks
      // TODO: Implement proper offline render for VSTi tracks

      // Check if track has audio items
      int (*CountTrackMediaItems)(MediaTrack *) =
          (int (*)(MediaTrack *))g_rec->GetFunc("CountTrackMediaItems");

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
        break;
      }

      // Perform DSP analysis directly on the track's audio
      // This reads the source audio (pre-FX for audio tracks)
      performDSPAnalysis(selectedTrackIndex, trackName, (float)analysisLength,
                         false);

      // Re-select the original track
      if (SetTrackSelected && selectedTrack) {
        SetTrackSelected(selectedTrack, true);
      }

      // Update arrange view
      void (*UpdateArrange)() = (void (*)())g_rec->GetFunc("UpdateArrange");
      if (UpdateArrange) {
        UpdateArrange();
      }
    }
    break;
  case MAGDA_CMD_TEST_EXECUTE_ACTION:
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
    break;
  default:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Unknown command\n");
    }
    break;
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
  if (command == MAGDA_MENU_CMD_ID || command == MAGDA_CMD_OPEN ||
      command == MAGDA_CMD_LOGIN || command == MAGDA_CMD_SETTINGS ||
      command == MAGDA_CMD_ABOUT || command == MAGDA_CMD_SCAN_PLUGINS ||
      command == MAGDA_CMD_ANALYZE_TRACK ||
      command == MAGDA_CMD_TEST_EXECUTE_ACTION) {
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

    // "Open MAGDA" item
    subMi.dwTypeData = (char *)"Open MAGDA";
    subMi.wID = MAGDA_CMD_OPEN;
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

    // Separator
    subMi.fMask = MIIM_TYPE;
    subMi.fType = MFT_SEPARATOR;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "Settings" item
    subMi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    subMi.fType = MFT_STRING;
    subMi.fState = MFS_UNCHECKED;
    subMi.dwTypeData = (char *)"Settings...";
    subMi.wID = MAGDA_CMD_SETTINGS;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "About" item
    subMi.dwTypeData = (char *)"About MAGDA...";
    subMi.wID = MAGDA_CMD_ABOUT;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // Separator
    subMi.fMask = MIIM_TYPE;
    subMi.fType = MFT_SEPARATOR;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

    // "Plugins" item (opens plugin alias window)
    subMi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    subMi.fType = MFT_STRING;
    subMi.fState = MFS_UNCHECKED;
    subMi.dwTypeData = (char *)"Plugins...";
    subMi.wID = MAGDA_CMD_SCAN_PLUGINS;
    InsertMenuItem(hSubMenu, GetMenuItemCount(hSubMenu), true, &subMi);

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
    if (g_imguiChat) {
      delete g_imguiChat;
      g_imguiChat = nullptr;
    }
    if (g_chatWindow) {
      delete g_chatWindow;
      g_chatWindow = nullptr;
    }
    if (g_loginWindow) {
      delete g_loginWindow;
      g_loginWindow = nullptr;
    }
    if (g_settingsWindow) {
      delete g_settingsWindow;
      g_settingsWindow = nullptr;
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

  // Register actions for all menu items
  gaccel_register_t gaccel_open = {{0, 0, MAGDA_CMD_OPEN}, "MAGDA: Open MAGDA"};
  gaccel_register_t gaccel_login = {{0, 0, MAGDA_CMD_LOGIN}, "MAGDA: Login"};
  gaccel_register_t gaccel_settings = {{0, 0, MAGDA_CMD_SETTINGS},
                                       "MAGDA: Settings"};
  gaccel_register_t gaccel_about = {{0, 0, MAGDA_CMD_ABOUT}, "MAGDA: About"};
  gaccel_register_t gaccel_scan_plugins = {{0, 0, MAGDA_CMD_SCAN_PLUGINS},
                                           "MAGDA: Plugins"};
  gaccel_register_t gaccel_analyze_track = {{0, 0, MAGDA_CMD_ANALYZE_TRACK},
                                            "MAGDA: Analyze Selected Track"};
  gaccel_register_t gaccel_test_execute = {
      {0, 0, MAGDA_CMD_TEST_EXECUTE_ACTION}, "MAGDA: Test Execute Action"};

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
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: ImGui chat initialized (ReaImGui available)\n");
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
