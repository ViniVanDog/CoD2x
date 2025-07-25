#define _WIN32_WINNT 0x0600

#include "admin.h"

#include <windows.h>
#include <Shlobj.h>

#include "shared.h"
#include "main.h"
#include "system.h"
#include "../shared/cod2_dvars.h"

/**
 * Check if the current process is running with elevated privileges.
 * Process must be running with elevated privileges to write to directories that are restricted on newer Windows.
 * This will fix the issues with VirtualStore folder that contains downloaded .iwd file outside of the game directory.
 * It will also enable to auto-update the CoD2x by downloading an dll file from the server.
 */
bool admin_isElevated() {
    // Open the current process token
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        SHOW_ERROR_WITH_LAST_ERROR("Failed to open process token.");
        return false; // Assume not admin
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
    return false;
}


bool admin_isPathUnderVirtualStore(const char* fullPath)
{
    char userAppData[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, userAppData) != S_OK)
        return false;

    char virtualStoreRoot[MAX_PATH];
    snprintf(virtualStoreRoot, MAX_PATH, "%s\\VirtualStore", userAppData);

    size_t len = strlen(virtualStoreRoot);
    bool isUnderVirtualStore = (_strnicmp(fullPath, virtualStoreRoot, len) == 0);

    /*if (isUnderVirtualStore) {
        MessageBoxA(NULL, fullPath, "Path is under VirtualStore", MB_OK | MB_ICONWARNING);
    }*/

    return isUnderVirtualStore;
}

bool admin_canWriteToFolderWithoutVirtualization(const char* path)
{
    char testFile[MAX_PATH];
    snprintf(testFile, MAX_PATH, "%s\\__test__.tmp", path);

    HANDLE hFile = CreateFileA(testFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        //MessageBoxA(NULL, "Failed to create test file for write access check.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    char resolved[MAX_PATH] = {0};
    DWORD len = GetFinalPathNameByHandleA(hFile, resolved, MAX_PATH, 0);
    CloseHandle(hFile);

    // Strip prefix "\\?\" if present
    if (strncmp(resolved, "\\\\?\\", 4) == 0)
        memmove(resolved, resolved + 4, strlen(resolved + 4) + 1);

    bool isVirtualStore = (len > 0 && admin_isPathUnderVirtualStore(resolved));

    return !isVirtualStore;
}


void admin_init() {

    // If the process is already running with elevated privileges, exit
    if (admin_isElevated()) {
        //MessageBoxA(NULL, "Running as admin", "CoD2x - Admin check", MB_ICONINFORMATION | MB_OK);
        return;
    }

    // Process is not elevated, but the game directory is writable without the Virtual Store redirection
    if (admin_canWriteToFolderWithoutVirtualization(EXE_DIRECTORY_PATH)) {
        return;
    }
    
    // If running under Wine, do not prompt for admin rights
    if (SYS_WINE_BUILD[0] != '\0') {
        return;
    }

    char errorMessage[1024];
    snprintf(errorMessage, sizeof(errorMessage),
        "Call of Duty 2 requires write access to the game directory.\n"
        "\n"
        "Please do one of the following:\n"
        "- Move the game directory to a location with write access.\n"
        "- Run the game with elevated administrator permissions.\n"
        "- Run the game with compatibility mode set to Windows XP.\n"
        "\n"
        "This is needed because the game needs to write files to the game installation directory, which is not allowed by default on newer Windows versions.\n"
        "\n"
        "Its recommended to move the game directory to a location with write access (e.g. C:\\Games\\Call of Duty 2) and uncheck administrator and compatibility mode for maximum security, if possible.\n"
        "\n"
        "Current game directory: \n"
        "%s\n"
        , EXE_DIRECTORY_PATH);

    // Show error message
    MessageBoxA(NULL, errorMessage, "Error", MB_ICONERROR);

    // Ask user if they want to restart the game with administrator privileges
    int result = MessageBoxA(NULL, 
        "Would you like to restart the game with administrator privileges?\n\n"
        "Warning:\n"
        "For security reasons, this is not recommended.\n"
        "The recommended way is to move the game directory to a location with write access (e.g. C:\\Games\\Call of Duty 2) and uncheck administrator and compatibility mode for maximum security, if possible.\n\n",
        "Run as administrator", MB_YESNO | MB_ICONQUESTION);

    if (result == IDYES) {

        // Set up the SHELLEXECUTEINFO structure
        SHELLEXECUTEINFOA sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = "runas";
        sei.lpFile = EXE_PATH;
        sei.lpParameters = EXE_COMMAND_LINE;
        sei.nShow = SW_SHOWNORMAL;

        // Relaunch the application with elevated privileges
        if (!ShellExecuteExA(&sei)) {
            MessageBoxA(NULL, "Failed to restart as administrator.", "Error", MB_ICONERROR);
        }
    }
    
    ExitProcess(0);
}