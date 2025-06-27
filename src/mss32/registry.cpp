#include "registry.h"

#include <windows.h>

#include "shared.h"
#include "../shared/version.h"

// Flag to indicate if the version has changed
bool registry_version_changed = false;

// Buffer to store the previous version
// If the buffer is empty, it means this is the first run of CoD2x or the version before 1.4.4.2 or lower (version change was introduced in 1.4.4.3)
char registry_previous_version[64] = {0}; 
bool registry_version_downgrade = false; // Flag to indicate if the version has been downgraded


bool registry_check() {

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


    RegCloseKey(hKey);

    return true;
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