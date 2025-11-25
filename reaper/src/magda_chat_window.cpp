#include "magda_chat_window.h"
#include "magda_chat_resource.h"
#include <cstring>

#ifndef _WIN32
#include <WDL/wdltypes.h>  // Defines GWLP_USERDATA for macOS
#endif

extern reaper_plugin_info_t *g_rec;
extern HINSTANCE g_hInst;

// Control IDs
#define IDC_QUESTION_INPUT 1001
#define IDC_QUESTION_DISPLAY 1002
#define IDC_REPLY_DISPLAY 1003
#define IDC_SEND_BUTTON 1004

MagdaChatWindow::MagdaChatWindow() 
    : m_hwnd(nullptr)
    , m_hwndQuestionInput(nullptr)
    , m_hwndQuestionDisplay(nullptr)
    , m_hwndReplyDisplay(nullptr)
    , m_hwndSendButton(nullptr)
{
}

MagdaChatWindow::~MagdaChatWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void MagdaChatWindow::Show(bool toggle) {
    if (!g_rec) return;
    
    if (m_hwnd && IsWindowVisible(m_hwnd)) {
        if (toggle) {
            Hide();
        } else {
            SetForegroundWindow(m_hwnd);
        }
        return;
    }
    
    if (!m_hwnd) {
        // Create a modeless dialog - SWS pattern
        CreateDialogParam(
            g_hInst,
            MAKEINTRESOURCE(IDD_MAGDA_CHAT),
            NULL,  // NULL parent = top-level floating window
            sDialogProc,
            (LPARAM)this
        );
    }
    
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetFocus(m_hwnd);
    }
}

void MagdaChatWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

// Static dialog proc - SWS pattern
INT_PTR WINAPI MagdaChatWindow::sDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Get 'this' pointer from GWLP_USERDATA (SWS pattern)
    MagdaChatWindow* pObj = (MagdaChatWindow*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    if (!pObj && uMsg == WM_INITDIALOG) {
        // Store 'this' pointer from lParam (passed from CreateDialogParam)
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        pObj = (MagdaChatWindow*)lParam;
        pObj->m_hwnd = hwndDlg;
    }
    return pObj ? pObj->DialogProc(uMsg, wParam, lParam) : 0;
}

// Instance dialog proc - handles all messages
INT_PTR MagdaChatWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            // Get control handles
            m_hwndQuestionDisplay = GetDlgItem(m_hwnd, IDC_QUESTION_DISPLAY);
            m_hwndReplyDisplay = GetDlgItem(m_hwnd, IDC_REPLY_DISPLAY);
            m_hwndQuestionInput = GetDlgItem(m_hwnd, IDC_QUESTION_INPUT);
            m_hwndSendButton = GetDlgItem(m_hwnd, IDC_SEND_BUTTON);
            
            // Validate all controls were created
            if (!m_hwndQuestionDisplay || !m_hwndReplyDisplay || !m_hwndQuestionInput || !m_hwndSendButton) {
                return FALSE;
            }
            
            // Get window size and update layout
            RECT r;
            GetClientRect(m_hwnd, &r);
            UpdateLayout(r.right - r.left, r.bottom - r.top);
            
            // Add welcome messages
            AddQuestion("Welcome! Type your questions here.\n\n");
            AddReply("MAGDA: Ready to help! Your responses will appear here.\n\n");
            
            return TRUE;
        }
            
        case WM_COMMAND:
            OnCommand(LOWORD(wParam), HIWORD(wParam));
            return 0;
            
        case WM_SIZE: {
            RECT r;
            GetClientRect(m_hwnd, &r);
            int width = r.right - r.left;
            int height = r.bottom - r.top;
            if (width > 100 && height > 100) {
                UpdateLayout(width, height);
            }
            return 0;
        }
            
        case WM_CLOSE:
            Hide();
            return 0;
            
        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;
            
        default:
            return 0;
    }
}

void MagdaChatWindow::OnCommand(int command, int notifyCode) {
    switch (command) {
        case IDC_SEND_BUTTON:
            OnSendMessage();
            break;
            
        case IDOK:  // Enter key in input field
            if (m_hwndQuestionInput && GetFocus() == m_hwndQuestionInput) {
                OnSendMessage();
            }
            break;
    }
}

void MagdaChatWindow::OnSendMessage() {
    if (!m_hwndQuestionInput) return;
    
    char buffer[1024];
    buffer[0] = '\0';
    GetWindowText(m_hwndQuestionInput, buffer, sizeof(buffer));
    
    if (strlen(buffer) > 0) {
        // Add question to question pane
        if (m_hwndQuestionDisplay) {
            AddQuestion(buffer);
            AddQuestion("\n");
        }
        
        // Clear input
        if (m_hwndQuestionInput) {
            SetWindowText(m_hwndQuestionInput, "");
        }
        
        // TODO: Process command and add response to reply pane
        if (m_hwndReplyDisplay) {
            char response[2048];
            snprintf(response, sizeof(response), "MAGDA: Command received: %s\n(Processing not yet implemented)\n\n", buffer);
            AddReply(response);
        }
    }
}

void MagdaChatWindow::AddQuestion(const char* question) {
    if (!m_hwndQuestionDisplay || !question) return;
    
    int len = GetWindowTextLength(m_hwndQuestionDisplay);
    SendMessage(m_hwndQuestionDisplay, EM_SETSEL, len, len);
    SendMessage(m_hwndQuestionDisplay, EM_REPLACESEL, FALSE, (LPARAM)question);
    int newLen = GetWindowTextLength(m_hwndQuestionDisplay);
    SendMessage(m_hwndQuestionDisplay, EM_SETSEL, newLen, newLen);
}

void MagdaChatWindow::AddReply(const char* reply) {
    if (!m_hwndReplyDisplay || !reply) return;
    
    int len = GetWindowTextLength(m_hwndReplyDisplay);
    SendMessage(m_hwndReplyDisplay, EM_SETSEL, len, len);
    SendMessage(m_hwndReplyDisplay, EM_REPLACESEL, FALSE, (LPARAM)reply);
    int newLen = GetWindowTextLength(m_hwndReplyDisplay);
    SendMessage(m_hwndReplyDisplay, EM_SETSEL, newLen, newLen);
}

void MagdaChatWindow::UpdateLayout(int width, int height) {
    if (!m_hwnd) return;
    
    // Update layout when window is resized
    if (width < 200) width = 200;
    if (height < 100) height = 100;
    
    int padding = 10;
    int inputHeight = 30;
    int buttonWidth = 70;
    int buttonHeight = 30;
    int spacing = 10;
    
    // Calculate available height for display panes
    int displayHeight = height - padding * 3 - inputHeight;
    if (displayHeight < 50) displayHeight = 50;
    
    // Calculate width for each display pane
    int paneWidth = (width - padding * 3 - spacing) / 2;
    if (paneWidth < 100) paneWidth = 100;
    
    // Question display: left pane
    if (m_hwndQuestionDisplay) {
        SetWindowPos(m_hwndQuestionDisplay, NULL, 
                    padding, padding, 
                    paneWidth, displayHeight, 
                    SWP_NOZORDER);
    }
    
    // Reply display: right pane
    if (m_hwndReplyDisplay) {
        SetWindowPos(m_hwndReplyDisplay, NULL, 
                    padding + paneWidth + spacing, padding, 
                    paneWidth, displayHeight, 
                    SWP_NOZORDER);
    }
    
    // Input field: bottom
    if (m_hwndQuestionInput) {
        int inputY = height - padding - inputHeight;
        int inputWidth = width - padding * 2 - buttonWidth - spacing;
        if (inputWidth < 50) inputWidth = 50;
        SetWindowPos(m_hwndQuestionInput, NULL, 
                    padding, inputY, 
                    inputWidth, inputHeight, 
                    SWP_NOZORDER);
    }
    
    // Send button: bottom right
    if (m_hwndSendButton) {
        int buttonY = height - padding - buttonHeight;
        SetWindowPos(m_hwndSendButton, NULL, 
                    width - padding - buttonWidth, buttonY, 
                    buttonWidth, buttonHeight, 
                    SWP_NOZORDER);
    }
}
