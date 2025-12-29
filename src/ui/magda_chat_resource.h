// Dialog resource for MAGDA Chat window
// This defines the controls programmatically using SWELL's resource system

#ifndef MAC
#define MAC
#endif

#include "../WDL/WDL/swell/swell-dlggen.h"

// Control IDs - must be defined before the resource
#define IDD_MAGDA_CHAT 3000
#define IDC_QUESTION_DISPLAY 1002
#define IDC_REPLY_DISPLAY 1003
#define IDC_QUESTION_INPUT 1001
#define IDC_SEND_BUTTON 1004
#define IDC_REQUEST_HEADER 1005
#define IDC_RESPONSE_HEADER 1006
#define IDC_STATUS_FOOTER 1007
#define IDC_CONTROLS_HEADER 1008
#define IDC_BTN_MIX_ANALYSIS 1010
#define IDC_BTN_MASTER_ANALYSIS 1011
#define IDC_BTN_GAIN_STAGING 1012
#define IDC_BTN_HOUSEKEEPING 1013

// Define the dialog resource
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_CHAT, 0, "MAGDA Chat", 1000, 600, 1.0)

BEGIN
// Request header (left column header)
LTEXT "REQUEST", IDC_REQUEST_HEADER, 10, 5, 300,
    20

    // Response header (middle column header)
    LTEXT "RESPONSE",
    IDC_RESPONSE_HEADER, 320, 5, 300,
    20

    // Controls header (right column header)
    LTEXT "CONTROLS",
    IDC_CONTROLS_HEADER, 630, 5, 150,
    20

    // Request display (left pane) - multiline read-only edit
    EDITTEXT IDC_QUESTION_DISPLAY,
    10, 28, 300, 500,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

            // Response display (middle pane) - multiline read-only edit
            EDITTEXT IDC_REPLY_DISPLAY,
    320, 28, 300, 500,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

        // Controls buttons (right pane) - reduced set
        PUSHBUTTON "Mix Analysis",
    IDC_BTN_MIX_ANALYSIS, 630, 30, 140, 28 PUSHBUTTON "Master Analysis", IDC_BTN_MASTER_ANALYSIS,
    630, 62, 140, 28 PUSHBUTTON "Gain Staging", IDC_BTN_GAIN_STAGING, 630, 100, 140,
    28 PUSHBUTTON "Housekeeping", IDC_BTN_HOUSEKEEPING, 630, 138, 140,
    28

    // Question input field (bottom, spans chat panes)
    EDITTEXT IDC_QUESTION_INPUT,
    10, 535, 600, 25,
    ES_AUTOHSCROLL | WS_BORDER

        // Send button
        PUSHBUTTON "Send",
    IDC_SEND_BUTTON, 620, 535, 50,
    25

    // Status footer
    LTEXT "Status: Checking...",
    IDC_STATUS_FOOTER, 10, 570, 780,
    20

    END

    SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_CHAT)
