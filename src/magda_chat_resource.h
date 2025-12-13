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

// Define the dialog resource
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_CHAT, 0, "MAGDA Chat", 1000, 600,
                                   1.0)

BEGIN
// Request header (left column header)
LTEXT "REQUEST", IDC_REQUEST_HEADER, 10, 5, 485,
    20

    // Response header (right column header)
    LTEXT "RESPONSE",
    IDC_RESPONSE_HEADER, 505, 5, 485,
    20

    // Request display (left pane) - multiline read-only edit
    EDITTEXT IDC_QUESTION_DISPLAY,
    10, 28, 485, 500,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

            // Response display (right pane) - multiline read-only edit
            EDITTEXT IDC_REPLY_DISPLAY,
    505, 28, 485, 500,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

            // Question input field (bottom, spans both panes)
            EDITTEXT IDC_QUESTION_INPUT,
    10, 535, 920, 25,
    ES_AUTOHSCROLL | WS_BORDER

        // Send button
        PUSHBUTTON "Send",
    IDC_SEND_BUTTON, 940, 535, 50,
    25

    // Status footer
    LTEXT "Status: Checking...",
    IDC_STATUS_FOOTER, 10, 570, 980,
    20

    END

    SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_CHAT)
