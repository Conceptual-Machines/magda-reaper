#pragma once

#ifndef MAC
#define MAC
#endif

#include "../WDL/WDL/swell/swell-dlggen.h"

// Dialog resource IDs
#define IDD_MAGDA_LOGIN 2000

// Control IDs
#define IDC_EMAIL_INPUT 2001
#define IDC_PASSWORD_INPUT 2002
#define IDC_LOGIN_BUTTON 2003
#define IDC_CANCEL_BUTTON 2004
#define IDC_STATUS_LABEL 2005
#define IDC_STATUS_ICON 2006

// Custom message for login completion
#define WM_LOGIN_COMPLETE (WM_USER + 100)

// Dialog resource definition using SWELL
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_LOGIN, 0, "MAGDA Login", 400, 220,
                                   1.0)
BEGIN
// Email label
LTEXT "Email:", -1, 20, 55, 60,
    20

    // Email input field
    EDITTEXT IDC_EMAIL_INPUT,
    90, 53, 280, 22,
    ES_AUTOHSCROLL | WS_BORDER |
        WS_VISIBLE

        // Password label
        LTEXT "Password:",
    -1, 20, 90, 60,
    20

    // Password input field (ES_PASSWORD hides the input)
    EDITTEXT IDC_PASSWORD_INPUT,
    90, 88, 280, 22,
    ES_AUTOHSCROLL | WS_BORDER |
        ES_PASSWORD

        // Status icon (static text control for icon character)
        LTEXT "",
    IDC_STATUS_ICON, 20, 125, 20,
    20

    // Status label
    LTEXT "",
    IDC_STATUS_LABEL, 50, 125, 320,
    20

    // Login button
    PUSHBUTTON "Login",
    IDC_LOGIN_BUTTON, 200, 165, 80,
    30

    // Cancel button
    PUSHBUTTON "Cancel",
    IDC_CANCEL_BUTTON, 290, 165, 80,
    30 END SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_LOGIN)
