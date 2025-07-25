#include "registry.h"

#include <windows.h>

#include "shared.h"
#include "main.h"
#include "../shared/version.h"

// Flag to indicate if the version has changed
bool registry_version_changed = false;

// Buffer to store the previous version
// If the buffer is empty, it means this is the first run of CoD2x or the version before 1.4.4.2 or lower (version change was introduced in 1.4.4.3)
char registry_previous_version[64] = {0}; 
bool registry_version_downgrade = false; // Flag to indicate if the version has been downgraded

extern bool admin_isElevated();


/**
 * Function to migrate registry settings from VirtualStore to real registry location.
 * If user run the game without admin rights, the game settings are automatically redirected to the VirtualStore folder by Windows to prevent access denied errors.
 * Once the user runs the game with admin rights, user will use outdated settings in the real registry location - we want to avoid this.
 * This function will copy all values from the VirtualStore to the real registry location and delete the VirtualStore key.
 */
bool registry_migrateVirtualStore() {
    
    // Not running as admin, so we cannot migrate the registry
    if (!admin_isElevated()) {
        return false;
    }

    const char* realKeyPath    = "SOFTWARE\\Activision\\Call of Duty 2"; // this might be automatically redirected to WOW6432Node on 64-bit systems
    const char* virtualStorePath32bit = "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Activision\\Call of Duty 2";
    const char* virtualStorePath64bit = "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Activision\\Call of Duty 2";

    HKEY hVirtKey = NULL, hRealKey = NULL;

    // Open VirtualStore key
    if (RegOpenKeyExA(HKEY_CURRENT_USER, virtualStorePath32bit, 0, KEY_READ | KEY_WRITE | KEY_SET_VALUE, &hVirtKey) != ERROR_SUCCESS) {
        if (RegOpenKeyExA(HKEY_CURRENT_USER, virtualStorePath64bit, 0, KEY_READ | KEY_WRITE | KEY_SET_VALUE, &hVirtKey) != ERROR_SUCCESS) {
            //MessageBoxA(NULL, "No VirtualStore key found for Call of Duty 2.", "Migration Info", MB_OK);
            return false; // No virtual store found, nothing to migrate
        }
    }

    // Create or open real key in HKLM
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, realKeyPath, 0, NULL, 0, KEY_WRITE, NULL, &hRealKey, NULL) != ERROR_SUCCESS) {
        RegCloseKey(hVirtKey);
        return false;
    }

    // Copy all values
    char valueName[256];
    BYTE data[1024];
    DWORD nameLen, dataLen, type;
    DWORD index = 0;
    while (true) {
        nameLen = sizeof(valueName);
        dataLen = sizeof(data);
        if (RegEnumValueA(hVirtKey, index++, valueName, &nameLen, NULL, &type, data, &dataLen) != ERROR_SUCCESS)
            break;
        RegSetValueExA(hRealKey, valueName, 0, type, data, dataLen);
    }
    RegCloseKey(hRealKey);


    // Delete all values in the VirtualStore key before deleting the key itself
    DWORD valueCount = 0, maxValueNameLen = 0;
    if (RegQueryInfoKeyA(hVirtKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, &maxValueNameLen, NULL, NULL, NULL) == ERROR_SUCCESS) {
        // maxValueNameLen does not include null terminator, so add 1
        char* valueNameBuf = (char*)malloc(maxValueNameLen + 1);
        if (valueNameBuf != NULL) {
            // Delete values from the end to avoid index shifting
            for (LONG i = valueCount - 1; i >= 0; --i) {
                DWORD nameLen = maxValueNameLen + 1;
                if (RegEnumValueA(hVirtKey, i, valueNameBuf, &nameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    RegDeleteValueA(hVirtKey, valueNameBuf);
                }
            }
            free(valueNameBuf);
        }
    }
    RegCloseKey(hVirtKey);

    // Now delete the VirtualStore key itself
    RegDeleteKeyA(HKEY_CURRENT_USER, virtualStorePath32bit);
    RegDeleteKeyA(HKEY_CURRENT_USER, virtualStorePath64bit);

    return true;
}


void registry_init() {

    // Migrate VirtualStore settings to real registry location (if running as admin)
    registry_migrateVirtualStore();

    
    HKEY hKey;
    LONG regResult = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", 0, KEY_READ | KEY_WRITE, &hKey);

    // If the game was not installed, the registry key will not exist.
    // If the key does not exist, create it
    if (regResult != ERROR_SUCCESS) {
        regResult = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL);
        if (regResult != ERROR_SUCCESS) {
            SHOW_ERROR("Failed to create registry key for Call of Duty 2.");
            exit(1);
        }
    }


    // Compare the current version with the one stored in the registry
    char regVersion[64] = {0};
    DWORD regVersionSize = sizeof(regVersion);
    LONG getResult = RegQueryValueExA(hKey, "COD2X_VERSION", NULL, NULL, (LPBYTE)regVersion, &regVersionSize);

    if (getResult == ERROR_SUCCESS) {
        // Value exists, check if it matches the current version
        if (strcmp(regVersion, APP_VERSION) != 0) {
            registry_version_changed = true;

            // If the version has changed, check if it is a downgrade
            if (regVersion[0] != '\0') {
                // Version in registry is higher than the current version
                if (version_compare(regVersion, APP_VERSION) > 0) {
                    registry_version_downgrade = true; // Version has been downgraded

                    char msg[512];
                    snprintf(msg, sizeof(msg),
                        "The CoD2x has been downgraded from a newer version.\n\n"
                        "Current version: %s\n"
                        "Previous version: %s\n\n"
                        "This may cause issues with the game. Please ensure you are using the correct version.\n\n"
                        "If you are having issues with the newer version, please contact the developers.",
                        APP_VERSION, regVersion);
                    MessageBoxA(NULL, msg, "Version downgrade", MB_OK | MB_ICONWARNING | MB_TOPMOST);
                }
            }
        }
        // Save the previous version for comparison
        strncpy(registry_previous_version, regVersion, sizeof(registry_previous_version) - 1);
        registry_previous_version[sizeof(registry_previous_version) - 1] = '\0';
    } else {
        // Value does not exist, so set the flag to write it
        registry_version_changed = true;
    }


    // This is not activated yet, but it might be useful in the future
    /*
    // Show warning only if elevation status changed from last time
    BOOL wasElevated = FALSE;
    DWORD wasElevatedSize = sizeof(wasElevated);
    LONG elevResult = RegQueryValueExA(hKey, "COD2X_WAS_ELEVATED", NULL, NULL, (LPBYTE)&wasElevated, &wasElevatedSize);

    bool isElevated = admin_isElevated();

    if (elevResult != ERROR_SUCCESS || wasElevated != isElevated) {
        // Save new elevation status
        RegSetValueExA(hKey, "COD2X_WAS_ELEVATED", 0, REG_DWORD, (const BYTE*)&isElevated, sizeof(isElevated));

        // Show warning only if now elevated
        if (isElevated) {
            const char* message =
                "Call of Duty 2 is running with administrator rights!\n\n"
                "CoD2x does not require to run the game with administrator rights or compatibility mode.\n\n"
                "Running the game with these settings can pose serious security risks. Elevated privileges allow the game and any loaded mods or scripts to make unrestricted changes to your system."
                    " With latest CoD2x changes its not needed to use these settings anymore. Removing them will improve your security.\n\n"
                "Recommended action:\n"
                "If you don't have any issues with the game, please remove the administrator and compatibility mode settings.\n"
                "To disable these settings: Right-click the game executable -> Properties -> Compatibility tab -> Uncheck all options.\n\n";
            MessageBoxA(NULL, message, "Security Warning", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }
    }
    */

    RegCloseKey(hKey);
}


/** Called every frame on frame start. */
void registry_frame() {
    if (registry_version_changed) {
        registry_version_changed = false; // Reset the flag

        // If the version has changed, update the registry
        HKEY hKey;
        LONG regResult = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", 0, KEY_READ | KEY_WRITE, &hKey);
        if (regResult == ERROR_SUCCESS) {
            regResult = RegSetValueExA(hKey, "COD2X_VERSION", 0, REG_SZ, (const BYTE*)APP_VERSION, (DWORD)strlen(APP_VERSION) + 1);
            if (regResult != ERROR_SUCCESS) {
                SHOW_ERROR("Failed to update COD2X_VERSION in registry.");
                exit(1);
            }
            RegCloseKey(hKey);
        } else {
            SHOW_ERROR("Failed to open registry key for Call of Duty 2.");
            exit(1);
        }
    }
}