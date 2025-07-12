#include "debug.h"

#include <windows.h>
#include <iostream>
#include <string>
#include <cstdio>

#include "shared.h"
#include "../shared/cod2_cmd.h"
#include "../shared/cod2_entity.h"
#include "../shared/cod2_dvars.h"

// Window class and title for the debug window.
#define DEBUG_WINDOW_CLASS "DebugWindowClass"
#define DEBUG_WINDOW_TITLE "Debug Window"

// New IDs and constants for toolbar.
#define TOOLBAR_HEIGHT 30
#define IDM_TOGGLE_UPDATE 1001

// Global handles for the debug window, toolbar, and its text control.
static HWND debug_hHWND = NULL;
static HWND debug_hToolbar = NULL;
static HWND debug_hEdit = NULL;
static bool debug_updateEnabled = true;
static HANDLE debug_hThread = NULL;
static DWORD debug_lastUpdateTime = 0;
static int debug_x = 0;
static int debug_y = 0;
static int debug_w = 0;
static int debug_h = 0;


// Window procedure for our debug window.
LRESULT CALLBACK debug_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
                RECT rc;
                GetClientRect(hwnd, &rc);
                // Resize the toolbar.
                if (debug_hToolbar) {
                    MoveWindow(debug_hToolbar, 0, 0, rc.right - rc.left, TOOLBAR_HEIGHT, TRUE);
                }
                // Resize the edit control to fill below the toolbar.
                if (debug_hEdit) {
                    MoveWindow(debug_hEdit, 0, TOOLBAR_HEIGHT, rc.right - rc.left, rc.bottom - rc.top - TOOLBAR_HEIGHT, TRUE);
                }
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_TOGGLE_UPDATE) {
                // Toggle the update flag.
                debug_updateEnabled = !debug_updateEnabled;
                // Update button text to reflect new state.
                SetWindowText(GetDlgItem(hwnd, IDM_TOGGLE_UPDATE), debug_updateEnabled ? "Disable Update" : "Enable Update");
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Create the debug window with a toolbar and a fullscreen text box.
void debug_createWindow(HINSTANCE hInstance) {
    // Register the window class.
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.lpfnWndProc   = debug_WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = DEBUG_WINDOW_CLASS;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    if (debug_x == 0 && debug_y == 0 && debug_w == 0 && debug_h == 0) {
        debug_x = CW_USEDEFAULT;
        debug_y = CW_USEDEFAULT;
        debug_w = 800;
        debug_h = 600;
    }

    // Create the window.
    debug_hHWND = CreateWindowEx(
        WS_EX_COMPOSITED | WS_EX_LEFT /*WS_EX_TOPMOST*/,
        DEBUG_WINDOW_CLASS,
        DEBUG_WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        debug_x, debug_y, debug_w, debug_h,
        NULL, NULL, hInstance, NULL
    );

    if (!debug_hHWND)
        return;
    
    // Create toolbar button for toggling updates.
    debug_hToolbar = CreateWindowEx(
        0,
        "BUTTON",
        "Disable Update", // Initial text when updateEnabled is true.
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 150, TOOLBAR_HEIGHT,
        debug_hHWND,
        (HMENU)IDM_TOGGLE_UPDATE,
        hInstance,
        NULL
    );

    // Create a multi-line, read-only edit control below the toolbar.
    debug_hEdit = CreateWindowEx(
        WS_EX_COMPOSITED | WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, TOOLBAR_HEIGHT, debug_w, debug_h - TOOLBAR_HEIGHT,
        debug_hHWND, (HMENU)1, hInstance, NULL
    );
    
    // Create a font with a specific size (e.g., 16 pixels height) and assign it to the edit control.
    HFONT hFont = CreateFont(
        -12,            // Height of font (negative value for character height)
        0,              // Average character width
        0,              // Angle of escapement
        0,              // Base-line orientation angle
        FW_NORMAL,      // Font weight
        FALSE,          // Italic attribute option
        FALSE,          // Underline attribute option
        FALSE,          // Strikeout attribute option
        DEFAULT_CHARSET,// Character set identifier
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Consolas"      // Typeface name
    );
    SendMessage(debug_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(debug_hHWND, SW_SHOWNORMAL);
    SetForegroundWindow(debug_hHWND);
    UpdateWindow(debug_hHWND);
}

// This thread creates and runs the debug window's message loop.
DWORD WINAPI debug_windowThread(LPVOID lpParam) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    debug_createWindow(hInstance);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(DEBUG_WINDOW_CLASS, hInstance);

    debug_hHWND = NULL;
    debug_hEdit = NULL;
    debug_hToolbar = NULL;
    debug_updateEnabled = true;

    return 0;
}

/** Dynamically closes the debug window, if open. */
void debug_close() {
    if (debug_hHWND != NULL) {
        // Post a WM_CLOSE message to gracefully close the window.
        PostMessage(debug_hHWND, WM_CLOSE, 0, 0);

        // Wait for the window to close and clean up handles.
        if (debug_hThread != NULL) {
            WaitForSingleObject(debug_hThread, 1000); // Wait for up to 1 second for the thread to finish.
            CloseHandle(debug_hThread);
            debug_hThread = NULL;
        }
    }
}

/** Reopens the debug window only if it is not currently active. */
void debug_reopen() {
    if (debug_hHWND == NULL) {
        DWORD threadId;
        debug_hThread = CreateThread(NULL, 0, debug_windowThread, NULL, 0, &threadId);
    }
}

/** Command to toggle the debug window visibility. */
void debug_cmd() {

    // Get x, y, w, h
    if (Cmd_Argc() == 5) {
        debug_x = atoi(Cmd_Argv(1));
        debug_y = atoi(Cmd_Argv(2));
        debug_w = atoi(Cmd_Argv(3));
        debug_h = atoi(Cmd_Argv(4));
    }

    if (debug_hHWND == NULL) {
        // If the window is not open, create it.
        debug_reopen();
    } else {
        // If the window is open, close it.
        debug_close();
    }
}



// Helper function to append a line to a buffer safely.
void debug_append_line(char* buffer, size_t bufferSize, const char* line) {
    strcat_s(buffer, bufferSize, line);
    strcat_s(buffer, bufferSize, "\r\n");
}

// Helper function to append a formatted line to a buffer safely.
void debug_append_linef(char* buffer, size_t bufferSize, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(temp, sizeof(temp), fmt, args);
    va_end(args);
    debug_append_line(buffer, bufferSize, temp);
}


/** Called every frame on frame start. */
void debug_frame() {
    DWORD currentTime = GetTickCount();
    if (currentTime - debug_lastUpdateTime < 25) {
        return;
    }
    debug_lastUpdateTime = currentTime;

    if (debug_hHWND && debug_hEdit && debug_updateEnabled) {
        char buffer[4096];
        buffer[0] = '\0';

        debug_append_linef(buffer, sizeof(buffer), "Tick count: %lu ms", GetTickCount());

        // Print server side information.
        if (sv_running->value.boolean) {

            // Print player entities
            for (int i = 0; i < MAX_GENTITIES; i++) {
                gentity_t* ent = &g_entities[i];
                if (ent->s.eType == ET_PLAYER) {
                    debug_append_linef(buffer, sizeof(buffer), "");
                    debug_append_linef(buffer, sizeof(buffer), "Entity %d:", i);
                    debug_append_linef(buffer, sizeof(buffer), "  Number: %d", ent->s.number);
                    debug_append_linef(buffer, sizeof(buffer), "  Client Num: %d", ent->s.clientNum);
                    debug_append_linef(buffer, sizeof(buffer), "  In use: %d", ent->r.inuse);
                    debug_append_linef(buffer, sizeof(buffer), "  Type: %d", ent->s.eType);
                    debug_append_linef(buffer, sizeof(buffer), "  Flags: 0x%x", ent->s.eFlags);
                    debug_append_linef(buffer, sizeof(buffer), "  Dmg Flags: 0x%x", ent->s.dmgFlags);
                    debug_append_linef(buffer, sizeof(buffer), "  Solid: 0x%x", ent->s.solid);
                    debug_append_linef(buffer, sizeof(buffer), "  Health: %d", ent->health);
                    debug_append_linef(buffer, sizeof(buffer), "  Max Health: %d", ent->maxHealth);
                    debug_append_linef(buffer, sizeof(buffer), "  Origin: (%.2f, %.2f, %.2f)", ent->s.pos.trBase[0], ent->s.pos.trBase[1], ent->s.pos.trBase[2]);
                    debug_append_linef(buffer, sizeof(buffer), "  Angles: (%.2f, %.2f, %.2f)", ent->s.apos.trBase[0], ent->s.apos.trBase[1], ent->s.apos.trBase[2]);
                    debug_append_linef(buffer, sizeof(buffer), "  Team: %d", ent->team);
                    debug_append_linef(buffer, sizeof(buffer), "  Weapon: %d", ent->s.weapon);
                    debug_append_linef(buffer, sizeof(buffer), "  Other Entity Num: %d", ent->s.otherEntityNum);
                    debug_append_linef(buffer, sizeof(buffer), "  Attacker Entity Num: %d", ent->s.attackerEntityNum);
                    debug_append_linef(buffer, sizeof(buffer), "  Ground Entity Num: %d", ent->s.groundEntityNum);
                    debug_append_linef(buffer, sizeof(buffer), "  Constant Light: %d", ent->s.constantLight);
                    debug_append_linef(buffer, sizeof(buffer), "  Events: %d %d %d %d", ent->s.events[0], ent->s.events[1], ent->s.events[2], ent->s.events[3]);
                    debug_append_linef(buffer, sizeof(buffer), "  EventParms: %d %d %d %d", ent->s.eventParms[0], ent->s.eventParms[1], ent->s.eventParms[2], ent->s.eventParms[3]);
                    debug_append_linef(buffer, sizeof(buffer), "  Event Time: %d", ent->eventTime);
                    debug_append_linef(buffer, sizeof(buffer), "  Event Sequence: %d", ent->s.eventSequence);
                    debug_append_linef(buffer, sizeof(buffer), "  Legs Anim: %d", ent->s.legsAnim);
                    debug_append_linef(buffer, sizeof(buffer), "  Torso Anim: %d", ent->s.torsoAnim);
                }
            }
        } else {
            debug_append_line(buffer, sizeof(buffer), "Server is not running.");
        }

        SetWindowTextA(debug_hEdit, buffer);
    }
}

/** Called once when hot-reloading is activated. */
void debug_unload() {
    #if DEBUG
    Cmd_RemoveCommand("debug");

    if (debug_hHWND != NULL) {
        
        // Update current window position and size.
        RECT rc;
        GetWindowRect(debug_hHWND, &rc);
        debug_x = rc.left;
        debug_y = rc.top;
        debug_w = rc.right - rc.left;
        debug_h = rc.bottom - rc.top;

        char cmd[64];
        sprintf_s(cmd, "wait 1\ndebug %d %d %d %d\n", debug_x, debug_y, debug_w, debug_h);
        Cbuf_AddText(cmd);

        debug_close();
    }
    #endif
}

/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void debug_init() {
    #if DEBUG
    Cmd_AddCommand("debug", debug_cmd);
    #endif
}
