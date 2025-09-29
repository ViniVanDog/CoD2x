#include "freeze.h"

#include <windows.h>
#include <stdio.h>
#include <dbghelp.h>

#include "shared.h"
#include "exception.h"
#include "../shared/cod2_dvars.h"
#include "../shared/cod2_cmd.h"

dvar_t* com_freezeWatch = NULL;
HANDLE crash_watchdogThreadHandle = NULL;
volatile DWORD freeze_lastHeartbeat = 0;   // Updated by the main thread
volatile bool freeze_exitThread = false;

extern volatile int exception_processCrashed;
extern volatile bool hotreload_requested;


DWORD WINAPI freeze_watchdogThreadProc(LPVOID lpParameter) {
    (void)lpParameter; // Unused parameter.
    while (!freeze_exitThread && !exception_processCrashed && !hotreload_requested && !com_errorEntered) {

        Sleep(1000);

        DWORD currentTime = GetTickCount();
        if (currentTime - freeze_lastHeartbeat > DEBUG_RELEASE(20000, 12000)) {

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
                while ((currentTime - freeze_lastHeartbeat) > 5000 && !freeze_exitThread && !exception_processCrashed && !hotreload_requested && !com_errorEntered) {
                    Sleep(1000);
                    currentTime = GetTickCount();
                }
            }
        }
    }
    return 0;
}


// Called when map is being loaded. In that time, COM_Frame is not called, so freeze heartbet is not being updated.
// Heartbeat is updated here to prevent the watchdog thread from thinking the game is frozen.
void Sys_LoadingKeepAlive() {
    logger_add("Sys_LoadingKeepAlive called");

    // Update the last heartbeat
    freeze_lastHeartbeat = GetTickCount();

    ASM_CALL(RETURN_VOID, 0x00465c80);
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
    if (com_freezeWatch->modified) {
        com_freezeWatch->modified = false;
        
        if (com_freezeWatch->value.boolean) {
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
    com_freezeWatch = Dvar_RegisterBool("com_freezeWatch", true, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));

    if (com_freezeWatch->value.boolean)
        com_freezeWatch->modified = true;
}


/** Called before the entry point is called. Used to patch the memory. */
void freeze_patch() {
    patch_call(0x004c0edd, (unsigned int)Sys_LoadingKeepAlive); // in CG_Init
    patch_call(0x004c107a, (unsigned int)Sys_LoadingKeepAlive); // in CG_Init
    patch_call(0x004bfbac, (unsigned int)Sys_LoadingKeepAlive); // in CG_RegisterGraphics
    patch_call(0x004c0032, (unsigned int)Sys_LoadingKeepAlive); // in CG_RegisterGraphics
}

