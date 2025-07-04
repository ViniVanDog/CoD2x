#include "admin.h"

#include <windows.h>
#include <stdio.h>

#include "shared.h"
#include "system.h"

/**
 * Check if the current process is running with elevated privileges.
 * Process must be running with elevated privileges to write to directories that are restricted on newer Windows.
 * This will fix the issues with VirtualStore folder that contains downloaded .iwd file outside of the game directory.
 * It will also enable to auto-update the CoD2x by downloading an dll file from the server.
 */
int admin_isElevated() {
    // Open the current process token
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to open process token.");
        return 0; // Assume not admin
    }

    // Try to check token elevation (Windows Vista and later)
    TOKEN_ELEVATION elevation;
    DWORD size = 0;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        CloseHandle(token);
        return elevation.TokenIsElevated;
    }

    // Fallback for Windows XP (or compatibility mode)
    // Check if the user is part of the Administrators group
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = NULL;

    if (AllocateAndInitializeSid(
            &ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
            &administratorsGroup)) {
        BOOL isAdmin = FALSE;
        CheckTokenMembership(NULL, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
        CloseHandle(token);
        return isAdmin;
    }

    // If all checks fail, assume not admin
    CloseHandle(token);
    return 0;
}


/**
 * Set the "Run as Administrator" compatibility flag in the registry for the specified executable.
 */
bool admin_setInRegistry(const char* exePath) {
    HKEY hKey;
    const char* registryPath = "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
    const char* value = "RUNASADMIN"; // Compatibility setting to run as administrator
    LONG result;

    // Open or create the Layers registry key
    result = RegCreateKeyExA(
        HKEY_CURRENT_USER,         // Scope: Current user
        registryPath,              // Path to the registry key
        0,                         // Reserved
        NULL,                      // Class (unused)
        REG_OPTION_NON_VOLATILE,   // Persistent key
        KEY_SET_VALUE,             // Permissions to set values
        NULL,                      // Security attributes
        &hKey,                     // Output handle to the key
        NULL                       // Disposition (not needed here)
    );

    if (result != ERROR_SUCCESS) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to open or create registry key.");
        return FALSE;
    }

    // Set the "Run as Administrator" flag for the specified executable
    result = RegSetValueExA(
        hKey,                      // Registry key handle
        exePath,                   // Key name (full path to the executable)
        0,                         // Reserved
        REG_SZ,                    // Value type: String
        (const BYTE*)value,        // Value data: RUNASADMIN
        (DWORD)(strlen(value) + 1) // Size of the value data (including null terminator)
    );

    if (result != ERROR_SUCCESS) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to set 'Run as Administrator' registry value.");
        RegCloseKey(hKey);
        return FALSE;
    }

    // Close the registry key
    RegCloseKey(hKey);

    return TRUE;
}


bool admin_check() {

    // If the process is already running with elevated privileges, exit
    if (admin_isElevated()) {
        return TRUE;
    }

    // If running under Wine, do not prompt for admin rights
    if (SYS_WINE_BUILD[0] != '\0') {
        return true;
    }
        
    // Get the full path of the current executable
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to get the executable path.");
        return 0;
    }

    int result = MessageBoxA(NULL, 
        "The game Call of Duty 2 requires an administrative privileges in order to run correctly.\n\n"
        "Would you like to set to always run the game with elevated administrator permissions?", 
        "CoD2x - Run as administrator", MB_YESNO | MB_ICONERROR);

    if (result == IDYES) {
        
        bool ok = admin_setInRegistry(exePath);

        if (!ok) {
            MessageBoxA(NULL, "Failed to set 'Run as Administrator'.\n\n"
            "Do the following manually:\n"
            " - right click on \"CoD2MP_s.exe\" and select \"Properties\".\n"
            " - click on \"Compatibility\" tab.\n"
            " - check \"Run this program as administrator\" in settings", "CoD2x - Run as administrator", MB_ICONINFORMATION); 

        } else {
            MessageBox(NULL, "The application is now set to always run as Administrator.\nThe game will be restarted...", "Success", MB_ICONINFORMATION);

            // Restart the application to apply changes
            ShellExecute(NULL, "open", exePath, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    
    ExitProcess(0);
    
    return FALSE;  
}