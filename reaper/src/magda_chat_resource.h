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

// Define the dialog resource
SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_MAGDA_CHAT, 0, "MAGDA Chat", 1000, 600, 1.0)

BEGIN
// Question display (left pane) - multiline read-only edit
EDITTEXT IDC_QUESTION_DISPLAY, 10, 10, 485, 540,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

            // Reply display (right pane) - multiline read-only edit
            EDITTEXT IDC_REPLY_DISPLAY,
    505, 10, 485, 540,
    ES_MULTILINE | ES_READONLY | ES_WANTRETURN | WS_VSCROLL |
        WS_BORDER

            // Question input field (bottom, spans both panes)
            EDITTEXT IDC_QUESTION_INPUT,
    10, 560, 920, 25,
    ES_AUTOHSCROLL | WS_BORDER

        // Send button
        PUSHBUTTON "Send",
    IDC_SEND_BUTTON, 940, 560, 50,
    25 END

    SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_MAGDA_CHAT)
