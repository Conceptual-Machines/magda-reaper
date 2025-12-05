#pragma once

#ifndef MAC
#define MAC
#endif

#include "../WDL/WDL/swell/swell-dlggen.h"

// Dialog resource IDs
#define IDD_MAGDA_SETTINGS 4000

// Control IDs
#define IDC_BACKEND_URL_INPUT 4001
#define IDC_BACKEND_URL_LABEL 4002
#define IDC_SAVE_BUTTON 4003

// State Filter controls
#define IDC_STATE_FILTER_MODE_LABEL 4005
#define IDC_STATE_FILTER_MODE_COMBO 4006
#define IDC_INCLUDE_EMPTY_TRACKS_CHECK 4007
#define IDC_MAX_CLIPS_PER_TRACK_LABEL 4008
#define IDC_MAX_CLIPS_PER_TRACK_INPUT 4009

// Dialog resource definition using SWELL
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_SETTINGS, 0, "MAGDA Settings", 450,
                                   300, 1.0)
BEGIN
// Backend URL label
LTEXT "Backend URL:", IDC_BACKEND_URL_LABEL, 20, 60, 100,
    20

    // Backend URL input field
    EDITTEXT IDC_BACKEND_URL_INPUT,
    130, 58, 300, 22,
    ES_AUTOHSCROLL | WS_BORDER |
        WS_VISIBLE

        // Help text
        LTEXT "Leave empty to use default (https://api.musicalaideas.com)",
    -1, 20, 85, 410,
    20

    // State Filter section label
    LTEXT "State Filtering (reduces token usage):",
    -1, 20, 115, 200,
    20

    // State Filter Mode label
    LTEXT "Filter Mode:",
    IDC_STATE_FILTER_MODE_LABEL, 20, 145, 100,
    20

    // State Filter Mode dropdown
    COMBOBOX IDC_STATE_FILTER_MODE_COMBO,
    130, 143, 300, 200,
    CBS_DROPDOWNLIST | WS_VSCROLL |
        WS_VISIBLE

        // Include empty tracks checkbox
        CONTROL "Include empty tracks",
    IDC_INCLUDE_EMPTY_TRACKS_CHECK, "Button", BS_AUTOCHECKBOX | WS_VISIBLE, 20,
    175, 200,
    20

    // Max clips per track label
    LTEXT "Max clips per track (0 = unlimited):",
    IDC_MAX_CLIPS_PER_TRACK_LABEL, 20, 205, 200,
    20

    // Max clips per track input
    EDITTEXT IDC_MAX_CLIPS_PER_TRACK_INPUT,
    230, 203, 80, 22,
    ES_AUTOHSCROLL | WS_BORDER | ES_NUMBER |
        WS_VISIBLE

        // Save button (centered)
        PUSHBUTTON "Save",
    IDC_SAVE_BUTTON, 185, 250, 80,
    30 END SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_SETTINGS)
