#include "magda_actions.h"
#include "magda_chat_window.h"
#include "magda_login_window.h"
#include "magda_plugin_scanner.h"
#include "magda_plugin_window.h"
#include "magda_settings_window.h"
#include "reaper_plugin.h"
// SWELL is already included by reaper_plugin.h

// Plugin instance handle
HINSTANCE g_hInst = nullptr;
reaper_plugin_info_t *g_rec = nullptr;

// Global chat window instance
static MagdaChatWindow *g_chatWindow = nullptr;
// Global login window instance
static MagdaLoginWindow *g_loginWindow = nullptr;
// Global settings window instance
static MagdaSettingsWindow *g_settingsWindow = nullptr;
// Global plugin scanner instance
MagdaPluginScanner *g_pluginScanner = nullptr;
// Global plugin window instance
MagdaPluginWindow *g_pluginWindow = nullptr;

// Command IDs for MAGDA menu items
#define MAGDA_MENU_CMD_ID 1000
#define MAGDA_CMD_OPEN 1001
#define MAGDA_CMD_LOGIN 1002
#define MAGDA_CMD_SETTINGS 1003
#define MAGDA_CMD_ABOUT 1004
#define MAGDA_CMD_SCAN_PLUGINS 1005
// Test/headless command IDs
#define MAGDA_CMD_TEST_EXECUTE_ACTION 2000

// Action callbacks for MAGDA menu items
void magdaAction(int command_id, int flag) {
  void (*ShowConsoleMsg)(const char *msg) =
      (void (*)(const char *))g_rec->GetFunc("ShowConsoleMsg");

  switch (command_id) {
  case MAGDA_CMD_OPEN:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Opening chat interface\n");
    }
    if (!g_chatWindow) {
      g_chatWindow = new MagdaChatWindow();
    }
    g_chatWindow->Show(true); // Toggle - show if hidden, hide if shown
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
