#include "main.h"

#include <windows.h>

#include "shared.h"
#include "hook.h"
#include "admin.h"
#include "mss32_original.h"


// Define a buffer to save the original bytes of the entry point
BYTE originalBytes[5];
void* originalEntryPoint = NULL;

HMODULE EXE_HMODULE = NULL;
char EXE_PATH[MAX_PATH] = {0}; 
char EXE_DIRECTORY_PATH[MAX_PATH] = {0};
char DLL_PATH[MAX_PATH] = {0}; 
bool DLL_HOTRELOAD = false; // set to true when the DLL was hot-reloaded in debug mode


/**
 * Load the CoD2x patches
 */
bool main_load() {  
    bool ok = FALSE;

    // Show warning message
    if (APP_VERSION_IS_TEST) {
        MessageBoxA(NULL, 
            "You successfully installed CoD2x " APP_VERSION ".\n\n"
            "Note that this is a test version, we recommend you to uninstall it after trying it!", "CoD2x warning", MB_OK | MB_ICONINFORMATION);
    }

    // Check if the user is an admin
    ok = admin_check();
    if (!ok) return FALSE;

    // Patch the game
    ok = hook_patch(); 
    if (!ok) return FALSE;

    return TRUE;
}


/**
 * New entry point for the our DLL.
 * This function is called when the application is started.
 * It gets called by the JMP instruction at the original entry point.
 */
void __cdecl main_newEntryPoint() {
    
    // Restore the original bytes at the original entry point
    patch_copy((unsigned int)originalEntryPoint, originalBytes, sizeof(originalBytes));

    // Run our code
    bool ok = main_load();
    if (!ok) {
        ExitProcess(EXIT_FAILURE);
        return;
    }

    // Jump back to the original entry point
    ((void (__cdecl *)(void))originalEntryPoint)();
}


/**
 * Hook the application's entry point
 */
bool main_patchEntryPoint() {

    // Load the original mss32.dll functions
    bool ok = mss32_load();
    if (!ok) return FALSE;

    // Check if process is SinglePlayer, if it is, exit
    char processName[MAX_PATH];
    GetModuleFileNameA(EXE_HMODULE, processName, MAX_PATH);
    if (strstr(processName, "CoD2SP_s.exe") != NULL) {
        return TRUE;
    }

    // Check if this is CoD2MP version 1.3
    char* cod2 = (char*)0x0059b6c0;
    if (strcmp(cod2, "pc_1.3_1_1") != 0) {
        MessageBoxA(NULL, 
            "CoD2x " APP_VERSION " is not installed correctly.\n\n"
            "You have to install patch 1.3 before installing CoD2x!", "CoD2x error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    // Get name of the DLL file
    char* lastBackslash = strrchr(DLL_PATH, '\\');
    if (!lastBackslash) {
        SHOW_ERROR("Failed to get DLL file name.");
        return FALSE;
    }

    // Check if this DLL file name is mss32.dll
    if (stricmp(lastBackslash + 1, APP_MODULE_NAME) != 0) {      
        #if DEBUG
            // If the DLL name is not mss32.dll, we can assume that the DLL is being hot-reloaded in debug mode
            DLL_HOTRELOAD = true;
            ok = hook_patch(); 
            if (!ok) {
                ExitProcess(EXIT_FAILURE);
                return false;
            }
            return TRUE;
        #else
            MessageBoxA(NULL, 
                "CoD2x " APP_VERSION " is not installed correctly.\n\n"
                "Invalid name of mss32.dll file!", "CoD2x error", MB_OK | MB_ICONERROR);
            return FALSE;
        #endif       
    }


    originalEntryPoint = (void*)0x0057db54; // Entry point of CoD2MP_s.exe

    // Save the original bytes at the entry point
    memcpy(originalBytes, originalEntryPoint, sizeof(originalBytes));

    // Patch the entry point with a jump to our new entry point
    patch_jump((unsigned int)originalEntryPoint, (unsigned int)&main_newEntryPoint);

    return TRUE;
}

/**
 * Get the module, executable path and directory
 */
bool main_getExeData() {
    char exePath[MAX_PATH];
    char exeDirectory[MAX_PATH];

    // Get the base address of the application
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to get application base address.");
        return false; 
    }
    EXE_HMODULE = hModule;

    // Get the full path of the current executable
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to get the executable path.");
        return false;
    }

    // Extract the directory from the executable path
    strncpy(exeDirectory, exePath, MAX_PATH);
    char* lastBackslash = strrchr(exeDirectory, '\\');
    if (lastBackslash) {
        *lastBackslash = '\0'; // Terminate the string at the last backslash
    } else {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to determine executable directory.");
        return false;
    }

    // Save the paths
    strncpy(EXE_PATH, exePath, MAX_PATH);
    strncpy(EXE_DIRECTORY_PATH, exeDirectory, MAX_PATH);


    // Get this DLL path
    HMODULE dllHandle = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&main_getExeData, &dllHandle)) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to get DLL handle");
        return false;
    }
    char dllPath[MAX_PATH];
    if (!GetModuleFileNameA(dllHandle, dllPath, MAX_PATH)) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to get current DLL path");
        return false;
    }
    strncpy(DLL_PATH, dllPath, MAX_PATH);
    

    return true;
}


/**
 * Entry point for the DLL.
 * When the system calls the DllMain function with the DLL_PROCESS_ATTACH value, the function returns TRUE if it succeeds or FALSE if initialization fails.
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) 
    {
        case DLL_PROCESS_ATTACH: 
        {
            bool ok;

            ok = main_getExeData();
            if (!ok) ExitProcess(EXIT_FAILURE);

            ok = main_patchEntryPoint();
            if (!ok) ExitProcess(EXIT_FAILURE);
            
            break;
        }

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}