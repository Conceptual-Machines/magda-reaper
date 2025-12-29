#pragma once

#ifndef MAC
#define MAC
#endif

#include "../WDL/WDL/swell/swell-dlggen.h"

// Dialog resource IDs
#define IDD_MAGDA_PLUGIN 5000

// Control IDs
#define IDC_ALIAS_LIST 5001
#define IDC_SCAN_BUTTON 5002
#define IDC_REFRESH_BUTTON 5003
#define IDC_STATUS_LABEL 5004

// Dialog resource definition using SWELL
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_PLUGIN, 0, "MAGDA Plugin Aliases", 900, 600, 1.0)
BEGIN
// Status label
LTEXT "Click 'Generate' to create aliases", IDC_STATUS_LABEL, 20, 5, 860,
    20

    // Plugin list view (table with Full Name and Aliases columns) - positioned
    // right after label - LVS_EDITLABELS allows editing
    CONTROL "",
    IDC_ALIAS_LIST, "SysListView32",
    LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL | WS_HSCROLL | WS_BORDER | WS_VISIBLE | WS_TABSTOP, 20,
    25, 860,
    445

    // Generate button (generates aliases)
    PUSHBUTTON "Generate",
    IDC_SCAN_BUTTON, 20, 465, 120,
    30

    // Save button (saves modified aliases)
    PUSHBUTTON "Save",
    IDC_REFRESH_BUTTON, 150, 465, 80, 30 END SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_PLUGIN)
