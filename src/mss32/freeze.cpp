#include "freeze.h"

#include <windows.h>
#include <stdio.h>
#include <dbghelp.h>

#include "shared.h"
#include "exception.h"

dvar_t* cl_freezeWatch = NULL;
HANDLE crash_watchdogThreadHandle = NULL;
volatile DWORD freeze_lastHeartbeat = 0;   // Updated by the main thread
volatile bool freeze_exitThread = false;

extern volatile bool exception_processCrashed;
extern volatile bool hotreload_requested;


DWORD WINAPI freeze_watchdogThreadProc(LPVOID lpParameter) {
    (void)lpParameter; // Unused parameter.
    while (!freeze_exitThread && !exception_processCrashed && !hotreload_requested) {

        Sleep(1000);

        DWORD currentTime = GetTickCount();
        if (currentTime - freeze_lastHeartbeat > 12000) {

            // Dont show messagebox for servers
            if (dedicated->value.boolean > 0) {
                continue;
            }

            // Message box asking if to ignore the freeze or create dump file and exit
            int result = MessageBox(NULL, 
                "Call of Duty 2 has frozen.\n\nDo you want to create a diagnostic file and exit the process?", 
                "Application frozen", MB_YESNO | MB_ICONERROR | MB_TOPMOST);
            
            if (result == IDYES) {
                bool ok = exception_createMiniDump(NULL);
                if (ok) {
                    MessageBox(NULL, "Diagnostic file 'CoD2MP_s.crash.dmp' saved into CoD2 folder.\nPlease send this file to the developers.",
                        "Application frozen", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                }
                ExitProcess(1);

            } else {
                // Ignore the freeze and wait for unfreeze
                while ((currentTime - freeze_lastHeartbeat) > 5000 && !freeze_exitThread && !exception_processCrashed && !hotreload_requested) {
                    Sleep(1000);
                    currentTime = GetTickCount();
                }
            }
        }
    }
    return 0;
}


/** Called every frame on frame start. */
void freeze_frame() {

    // For server just print the freeze time
    if (dedicated->value.boolean > 0) {
        DWORD deltaTime = GetTickCount() - freeze_lastHeartbeat;
        if (freeze_lastHeartbeat > 0 && (deltaTime) > 1000) {
            Com_DPrintf("Server frozen for %d ms\n", deltaTime);
        }
    }

    // Update the last heartbeat
    freeze_lastHeartbeat = GetTickCount();

    // Check if the cvar was modified
    if (cl_freezeWatch->modified) {
        cl_freezeWatch->modified = false;
        
        if (cl_freezeWatch->value.boolean) {
            // Create the watchdog thread.
            freeze_exitThread = false;
            crash_watchdogThreadHandle = CreateThread(NULL, 0, freeze_watchdogThreadProc, NULL, 0, NULL);
            if (crash_watchdogThreadHandle == NULL) {
                Com_Error(ERR_FATAL, "Failed to create watchdog thread.");
            }
        } else {
            // Terminate the watchdog thread.
            if (crash_watchdogThreadHandle != NULL) {
                freeze_exitThread = true;
                WaitForSingleObject(crash_watchdogThreadHandle, INFINITE);
                CloseHandle(crash_watchdogThreadHandle);
                crash_watchdogThreadHandle = NULL;
            }
        }
    }
}


/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void freeze_init() {
    cl_freezeWatch = Dvar_RegisterBool("cl_freezeWatch", true, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));

    if (cl_freezeWatch->value.boolean)
        cl_freezeWatch->modified = true;
}


