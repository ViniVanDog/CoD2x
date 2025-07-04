#include "system.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"

char SYS_WIN_BUILD[64] = {0};
char SYS_WIN_EDITION[64] = {0};
char SYS_WIN_DISPLAY_VERSION[32] = {0};
char SYS_WINE_VERSION[64] = {0};
char SYS_WINE_BUILD[64] = {0};
char SYS_VERSION_INFO[256] = {0};
char SYS_VM_NAME[64] = {0}; // Name of the virtualization platform, if detected


// Retrieves Windows version information from the registry to get the real data (to bypass compatibility modes).
void system_getWindowsVersionInfoFromRegistry() {
    HKEY hKey;
    LONG status;

    // Try 64-bit registry view first
    status = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &hKey
    );

    if (status != ERROR_SUCCESS) {
        // Fallback: try 32-bit view (or 32-bit OS)
        status = RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0,
            KEY_READ,
            &hKey
        );
    }

    if (status != ERROR_SUCCESS) {
        // Failed to open registry key
        return;
    }

    DWORD size;

    // Build number (required)
    size = sizeof(SYS_WIN_BUILD);
    RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, NULL, (LPBYTE)SYS_WIN_BUILD, &size);

    // EditionID (optional)
    size = sizeof(SYS_WIN_EDITION);
    RegQueryValueExA(hKey, "EditionID", NULL, NULL, (LPBYTE)SYS_WIN_EDITION, &size);

    // DisplayVersion (optional)
    size = sizeof(SYS_WIN_DISPLAY_VERSION);
    RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL, (LPBYTE)SYS_WIN_DISPLAY_VERSION, &size);

    RegCloseKey(hKey);
}

// Retrieves the Wine version string and build info if running under Wine.
bool system_getWineVersion() {
    SYS_WINE_VERSION[0] = '\0';

    HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
    if (!hNtDll) return false;

    typedef const char* (__cdecl *wine_get_version_t)(void);
    typedef const char* (__cdecl *wine_get_build_id_t)(void);

    wine_get_version_t wine_get_version = (wine_get_version_t)(void*)GetProcAddress(hNtDll, "wine_get_version");
    wine_get_build_id_t wine_get_build_id = (wine_get_build_id_t)(void*)GetProcAddress(hNtDll, "wine_get_build_id");

    if (!wine_get_version) return false;

    const char* version = wine_get_version();
    const char* build_id = wine_get_build_id ? wine_get_build_id() : NULL;

    // Copy strings
    if (version) {
        snprintf(SYS_WINE_VERSION, sizeof(SYS_WINE_VERSION), "%s", version);
    }
    if (build_id) {
        snprintf(SYS_WINE_BUILD, sizeof(SYS_WINE_BUILD), "%s", build_id);
    }

    return true;
}

// Checks if the system is running under a virtual machine by checking the BIOS information.
bool system_isVirtualMachineBios() {
    char buffer[256] = {0};
    DWORD bufferSize = sizeof(buffer);

    SYS_VM_NAME[0] = '\0';

    LSTATUS result = RegGetValueA(
        HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\BIOS",
        "SystemManufacturer",
        RRF_RT_REG_SZ,
        NULL,
        buffer,
        &bufferSize
    );

    if (result != ERROR_SUCCESS)
        return false;

    // Convert to lowercase for case-insensitive match
    CharLowerA(buffer);

    if (strstr(buffer, "vmware") != NULL) {
        strncpy(SYS_VM_NAME, "VMware", sizeof(SYS_VM_NAME) - 1);
        return true;
    }
    if (strstr(buffer, "virtualbox") != NULL) {
        strncpy(SYS_VM_NAME, "VirtualBox", sizeof(SYS_VM_NAME) - 1);
        return true;
    }
    if (strstr(buffer, "qemu") != NULL) {
        strncpy(SYS_VM_NAME, "QEMU", sizeof(SYS_VM_NAME) - 1);
        return true;
    }
    if (strstr(buffer, "microsoft corporation") != NULL) { // Hyper-V
        strncpy(SYS_VM_NAME, "Hyper-V", sizeof(SYS_VM_NAME) - 1);
        return true;
    }
    if (strstr(buffer, "parallels") != NULL) {
        strncpy(SYS_VM_NAME, "Parallels", sizeof(SYS_VM_NAME) - 1);
        return true;
    }

    return false;
}

void system_getCountryCode(char *buffer, int bufferSize) {
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buffer, bufferSize)) {
        snprintf(buffer, bufferSize, "??");
    }
}


void system_getInfo() {
    // Get Windows version info
    system_getWindowsVersionInfoFromRegistry();

    // Get Wine version if running under Wine
    bool isWine = system_getWineVersion();

    // Get virtualization info
    bool isVM = system_isVirtualMachineBios();

    // Get country code
    char countryCode[8] = {0};
    system_getCountryCode(countryCode, sizeof(countryCode)); 

    char virtualization[512];
    if (isWine) {
        snprintf(virtualization, sizeof(virtualization), "wine-%s-%s", SYS_WINE_VERSION, SYS_WINE_BUILD);
    } else if (isVM) {
        snprintf(virtualization, sizeof(virtualization), "VM-%s", SYS_VM_NAME);
    } else {
        snprintf(virtualization, sizeof(virtualization), "0");
    }

    // Compose final version info string
    // Format: "<build>-<edition>-<displayVersion>-<country>-<virtualization>"
    snprintf(SYS_VERSION_INFO, sizeof(SYS_VERSION_INFO), "%s\n%s\n%s\n%s\n%s", SYS_WIN_BUILD, SYS_WIN_EDITION, SYS_WIN_DISPLAY_VERSION, countryCode, virtualization);

    //MessageBoxA(NULL, SYS_VERSION_INFO, "System Info", MB_OK | MB_ICONINFORMATION);
}