#include "magda_chat_window.h"
#include "reaper_plugin.h"
// #include <ReaWrap/ReaperAPI.h>  // Temporarily disabled for debugging
// SWELL is already included by reaper_plugin.h

// Plugin instance handle
HINSTANCE g_hInst = nullptr;
reaper_plugin_info_t *g_rec = nullptr;

// Global chat window instance
static MagdaChatWindow *g_chatWindow = nullptr;

// Command IDs for MAGDA menu items
#define MAGDA_MENU_CMD_ID 1000
#define MAGDA_CMD_OPEN 1001
#define MAGDA_CMD_SETTINGS 1002
#define MAGDA_CMD_ABOUT 1003

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
  case MAGDA_CMD_SETTINGS:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Settings - TODO: Show settings dialog\n");
    }
    // TODO: Show settings dialog
    break;
  case MAGDA_CMD_ABOUT:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: About - TODO: Show about dialog\n");
    }
    // TODO: Show about dialog
    break;
  default:
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Unknown command\n");
    }
    break;
  }
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

    // Now that we're linking against SWELL stub, function pointers should be initialized
    // Add menu item to "Main extensions" - following SWS pattern exactly
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Adding menu item to Main extensions\n");
    }

    // Get menu item count right before inserting to ensure we're at the end
    // Other extensions might add items during the same hook call, so we check right before
    // inserting
    int itemCount = GetMenuItemCount(hMenu);
    if (ShowConsoleMsg) {
      char msg[512];
      snprintf(msg, sizeof(msg), "MAGDA: Initial menu count: %d items\n", itemCount);
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

    // Re-check count right before inserting (in case other extensions added items)
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
REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                                                      reaper_plugin_info_t *rec) {
  if (!rec) {
    // Extension is being unloaded
    if (g_chatWindow) {
      delete g_chatWindow;
      g_chatWindow = nullptr;
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
  void (*ShowConsoleMsg)(const char *msg) = (void (*)(const char *))rec->GetFunc("ShowConsoleMsg");

  // Always try to show a message - even if ShowConsoleMsg is NULL, we'll continue
  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA: Extension entry point called\n");
    ShowConsoleMsg("MAGDA: Testing console output...\n");
  }

  // Register actions for all menu items
  gaccel_register_t gaccel_open = {{0, 0, MAGDA_CMD_OPEN}, "MAGDA: Open MAGDA"};
  gaccel_register_t gaccel_settings = {{0, 0, MAGDA_CMD_SETTINGS}, "MAGDA: Settings"};
  gaccel_register_t gaccel_about = {{0, 0, MAGDA_CMD_ABOUT}, "MAGDA: About"};

  if (rec->Register("gaccel", &gaccel_open)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered 'Open MAGDA' action\n");
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

  // Register command hook to handle the actions
  // hookcommand2 signature: bool onAction(KbdSectionInfo *sec, int command, int val, int val2, int
  // relmode, HWND hwnd)
  typedef bool (*hookcommand2_t)(KbdSectionInfo *, int, int, int, int, HWND);
  hookcommand2_t hookcmd = [](KbdSectionInfo *sec, int command, int val, int val2, int relmode,
                              HWND hwnd) -> bool {
    if (command == MAGDA_MENU_CMD_ID || command == MAGDA_CMD_OPEN ||
        command == MAGDA_CMD_SETTINGS || command == MAGDA_CMD_ABOUT) {
      magdaAction(command, 0);
      return true; // handled
    }
    return false; // not handled
  };
  if (rec->Register("hookcommand2", (void *)hookcmd)) {
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Registered hookcommand2\n");
    }
  }

  // Call AddExtensionsMainMenu() - it takes no parameters and ensures Extensions menu exists
  typedef bool (*AddExtensionsMainMenuFunc)();
  AddExtensionsMainMenuFunc AddExtensionsMainMenu =
      (AddExtensionsMainMenuFunc)rec->GetFunc("AddExtensionsMainMenu");

  if (AddExtensionsMainMenu) {
    AddExtensionsMainMenu(); // Call with no parameters
    if (ShowConsoleMsg) {
      ShowConsoleMsg("MAGDA: Called AddExtensionsMainMenu()\n");
    }
  }

  // Don't use AddCustomizableMenu - we'll add the menu item directly to Extensions menu
  // via hookcustommenu instead

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

  // ReaWrap removed - using plain REAPER API via MagdaActions instead

  if (ShowConsoleMsg) {
    ShowConsoleMsg("MAGDA Reaper Extension loaded successfully!\n");
  }

  // Extension loaded successfully
  return 1;
}
}
