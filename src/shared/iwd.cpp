#include "iwd.h"
#include "shared.h"

#if COD2X_WIN32
    #include <windows.h>
#endif
#include <dirent.h>
#include <sys/stat.h>   // stat, mkdir
#include <cstdio>       // fopen, fwrite, fclose
#include <cerrno>       // errno
#include <cstring>      // strerror

#include "cod2_common.h"
#include "cod2_dvars.h"
#include "cod2_shared.h"
#include "cod2_file.h"
#include "cod2_cmd.h"
#if COD2X_WIN32
    #include "cod2_client.h"
#endif


// Macros to declare and use embedded files
// Windows strip the underscore prefix from symbol names, Linux does not
#if COD2X_WIN32
    #define EMBEDDED_FILE_DECLARATIONS(filename) \
        extern const unsigned char binary_##filename##_start[]; \
        extern const unsigned char binary_##filename##_end[];
    #define EMBEDDED_FILE_SYMBOLS(filename) \
        binary_##filename##_start, binary_##filename##_end
#endif
#if COD2X_LINUX
    #define EMBEDDED_FILE_DECLARATIONS(filename) \
        extern const unsigned char _binary_##filename##_start[]; \
        extern const unsigned char _binary_##filename##_end[];
    #define EMBEDDED_FILE_SYMBOLS(filename) \
        _binary_##filename##_start, _binary_##filename##_end
#endif

// Define embedded files
// The file name depends on the name used in CMakeLists.txt - file name is used with non-alphanumeric characters replaced by '_'
extern "C" {
    EMBEDDED_FILE_DECLARATIONS(iw_CoD2x_01_iwd)
}

#define NUM_IW_IWDS 25 // Original
#define NUM_IW_COD2X_IWDS 1 // New CoD2x iwds


/**
 * Cleanup newer or testing CoD2x IWD files
 */
static void iwd_cleanupCoD2xIwdFiles() {
    dvar_t* fs_basePath = Dvar_GetDvarByName("fs_basepath");
    if (!fs_basePath || !fs_basePath->value.string || fs_basePath->value.string[0] == '\0') {
        Com_Printf("CoD2x IWD cleanup: fs_basepath not set, skipping.\n");
        return;
    }

    const char* sep = WL("\\", "/");
    char mainDir[MAX_OSPATH * 2] = {0};
    snprintf(mainDir, sizeof(mainDir), "%s%smain", (const char*)fs_basePath->value.string, sep);

    auto shouldDelete = [](const char* filename) -> bool {
        // Only consider IWD files
        const char* dot = strrchr(filename, '.');
        if (!dot || FS_FilenameCompare(dot, ".iwd") != 0) return false;

        // Must start with iw_CoD2x_
        const char* prefix = "iw_CoD2x_";
        size_t prefixLen = strlen(prefix);
        if (I_strnicmp(filename, prefix, prefixLen) != 0) return false;

        // Expect two digits after prefix
        if (!isdigit((unsigned char)filename[prefixLen]) || !isdigit((unsigned char)filename[prefixLen + 1])) {
            return false;
        }
        int num = (filename[prefixLen] - '0') * 10 + (filename[prefixLen + 1] - '0');

        // Delete higher-numbered IWDs (downgrade protection)
        if (num > NUM_IW_COD2X_IWDS) return true;

        // For not-test versions, delete testing versions
        if (APP_VERSION_IS_TEST == 0) {
            // iw_CoD2x_xx_1.4.5.1.iwd
            // iw_CoD2x_xx_1.4.5.1-test.1.iwd
            const char* versionPart = filename + prefixLen + 2; // after the two digits
            if (*versionPart != '.') return true; // if it does not end with .iwd, delete it (it's a testing version)
        }

        return false;
    };


    DIR* dir = opendir(mainDir);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        const char* name = ent->d_name;

        // skip dot entries
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        // build full path and filter directories via stat
        char fullPath[MAX_OSPATH * 2] = {0};
        snprintf(fullPath, sizeof(fullPath), "%s%s%s", mainDir, sep, name);

        struct stat st;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            continue;
        }

        if (shouldDelete(name)) {
            #if COD2X_WIN32
                // Confirm on Windows
                if (IDYES != MessageBoxA(
                        NULL,
                        va("Found newer or testing CoD2x IWD that needs to be deleted:\n%s\n\nProceed with deletion?", fullPath),
                        "Confirm Deletion",
                        MB_ICONWARNING | MB_YESNO)) {
                    closedir(dir);
                    exit(0);
                    return;
                }
            #endif

            if (remove(fullPath) == 0) {
                Com_Printf("Deleted CoD2x IWD: %s\n", fullPath);
            } else {
                Com_Error(ERR_FATAL, "Failed to delete CoD2x IWD: %s (errno=%d)\n", fullPath, errno);
            }
        }
    }
    closedir(dir);

}


// Write embedded file if it doesn’t already exist
void iwd_extractFiles(const char* out_path, const unsigned char* start, const unsigned char* end) {
    size_t size = (size_t)(end - start);

    // Check if file already exists and has the correct size
    struct stat st;
    if (stat(out_path, &st) == 0) {
        if ((size_t)st.st_size == size) {
            return; // file already exists and size matches
        } else {
            // file exists but size does not match, delete it
            int result = remove(out_path);
            if (result != 0) {
                Com_Error(ERR_FATAL, "Error while extracting IWD file to '%s'. Error while deleting existing file: %s\n", out_path, strerror(errno));
                return;
            }
        }
    }

    FILE* f = fopen(out_path, "wb");
    if (!f) {
        Com_Error(ERR_FATAL, "Error while extracting IWD file to '%s'. Error while opening file: %s\n", out_path, strerror(errno));
        return;
    }

    size_t written = fwrite(start, 1, size, f);
    fclose(f);

    if (written != size) {
        Com_Error(ERR_FATAL, "Error while extracting IWD file to '%s'. Error while writing to file: %s\n", out_path, strerror(errno));
        return;
    }
}

/**
 * Extract embedded iwd file file into main folder
 */
void iwd_extractIwdFileToMain(const char* iwdFileName, const unsigned char* start, const unsigned char* end) {
    
    dvar_t* fs_basePath = Dvar_GetDvarByName("fs_basepath"); // installation directory of the game

    if (!fs_basePath || !fs_basePath->value.string || fs_basePath->value.string[0] == '\0') {
        Com_Error(ERR_FATAL, "Error while extracting IWD file '%s' into main folder. Dvar fs_basepath not found\n", iwdFileName);
        return;
    }

    const char* pathSeparator = WL("\\", "/");
    const char* testVersion = APP_VERSION_IS_TEST ? va("_%s", APP_VERSION) : "";
    // path/main/iw_CoD2x_01.iwd
    // path/main/iw_CoD2x_01_1.4.5.1-test.1.iwd - for test versions, file will be deleted
    const char* newFile = va("%s%smain%s%s%s.iwd", (const char*)fs_basePath->value.string, pathSeparator, pathSeparator, iwdFileName, testVersion);

    iwd_extractFiles(newFile, start, end);
}


/**
 * Is called when game needs to verify if particular iwd file is part of installation, not custom mod
 * Since we added new iw_CoD2x_01.iwd files, we need to make sure these files are accepted
 * iwd might be "main/zpam_maps_v4"
 * base might be "main"
 */
qboolean FS_iwIwd(char *iwd, const char *base)
{
	char *pszLoc;
	char szFile[MAX_QPATH];
	int i;

	for ( i = 0; i < NUM_IW_IWDS; ++i )
	{
        const char *filename = va("%s/iw_%02d", base, i); // check if it starts with this
		if ( !FS_FilenameCompare(iwd, filename) )
			return qtrue;
	}

    // CoD2x: New iwds
    for ( i = 0; i < NUM_IW_COD2X_IWDS; ++i )
    {
        const char *filename = va("%s/iw_CoD2x_%02d", base, i + 1); // check if it starts with this
        if ( !I_strnicmp(iwd, filename, strlen(filename)) )
            return qtrue;
    }
    // CoD2x: End

	pszLoc = strstr(iwd, "localized_");

	if ( pszLoc )
	{
		strcpy(szFile, iwd);
		szFile[pszLoc - iwd + 10] = 0;

		if ( !FS_FilenameCompare(szFile, va("%s/localized_", base)) )
		{
			strcpy(szFile, pszLoc + 10);
			I_strlwr(szFile);

			for ( i = 0; i < NUM_IW_IWDS; ++i )
			{
				if ( strstr(szFile, va("_iw%02d", i)) )
					return qtrue;
			}
		}
	}

	return qfalse;
}
qboolean FS_iwIwd_Win32() {
	char* iwd;  ASM( movr, iwd,  "edi" );
    char* base; ASM( movr, base, "ebx" );
    return FS_iwIwd(iwd, base);
}



#if COD2X_WIN32

// Windows only
// Called when the game needs to list files in a directory
char** Sys_ListFiles(char* extension, int32_t* numFiles, int32_t wantsubs) {
    // Load parameters from registers
    char* directory;
    char* filter;
    ASM( movr, directory, "eax" );
    ASM( movr, filter, "edx" );

    // Call the original function
    char** result;
    ASM_CALL(RETURN(result), 0x00463e70, 3, EAX(directory), EDX(filter), PUSH(extension), PUSH(numFiles), PUSH(wantsubs));
    // directory: D:\\CoD2x\\bin\\windows\\main
    // extension: "iwd"
    // result[0] = "iw_00.iwd"

    // When the game starts for the first time, load only the original IWD files
    // The main folder might contain mix of mods from different servers that might cause "iwd sum mismatch" errors when running the game
    // This will make sure these mods are not loaded at startup, but will be loaded when connecting to the game

    // No files in this folder (propably raw, devraw, etc)
    if (*numFiles <= 0) {
        return result;
    }

    // Read value via function to get value even if dvar is not registered yet
    bool dedicatedValue = Dvar_GetInt("dedicated");

    // Server
    // - load all, do not filter
    if (dedicatedValue > 0) {
        return result;
    }

    // Client connecting to server (not demo and not listen server)
    else if (clientState >= CLIENT_STATE_CONNECTING && demo_isPlaying == false && sv_running->value.boolean == false) {
        return result;
    }

    else {
        dvar_t* fs_game = Dvar_GetDvarByName("fs_game");
        if (!fs_game || (fs_game->value.string && fs_game->value.string[0] != '\0' && stricmp(fs_game->value.string, "main") != 0)) {
            return result; // mods are allowed, do not filter anything
        }

        // The original function created list of loaded iwd files
        // We will remove some entries from this list via filtering
        int writeIndex = 0;
        int i;
        for (i = 0; i < *numFiles; i++) {
            //Com_Printf("File: %s\n", result[i]);
            
            // Remove ".iwd" extension from result[i]
            char iwIwdName[MAX_QPATH];
            char* dot = strrchr(result[i], '.');
            int len = dot ? (int)(dot - result[i]) : (int)strlen(result[i]);
            snprintf(iwIwdName, sizeof(iwIwdName), "/%.*s", len, result[i]);
            
            // Is file "iw_00" - "iw_15" or starts with "localized_"
            bool isOriginalFile = FS_iwIwd(iwIwdName, "");

            // When replaying a demo, make sure all iwds used by the server are loaded
            if (isOriginalFile == false && COD2X_WIN32 && demo_isPlaying) {

                const char* serverinfo = CL_GetConfigString(CS_SYSTEMINFO);
                const char* sv_iwdNames = Info_ValueForKey(serverinfo, "sv_iwdNames"); // "zpam_maps_v4 zpam334 iw_16 iw_15 iw_14 iw_13 iw_12 iw_11 iw_10 iw_09 iw_08 iw_07 iw_06 iw_05 iw_04 iw_03 iw_02 iw_01".

                Cmd_TokenizeString(sv_iwdNames);

                int count = Cmd_Argc();
                if (count > 1024)
                    count = 1024;
                if (count > 0) {               
                    // Check if this iwd is in the list
                    for (int j = 0; j < count; j++) {
                        const char* demoIwdName = Cmd_Argv(j); // "zpam334"
                        if (FS_FilenameCompare(va("%s.iwd", demoIwdName), result[i]) == 0) {
                            Com_Printf("Required IWD from demo: %s.iwd\n", demoIwdName);
                            isOriginalFile = true;
                            break;
                        }
                    }
                }
            }

            // Listen server
            else if (isOriginalFile == false && COD2X_WIN32 && sv_running && sv_running->value.boolean && clientState != CLIENT_STATE_DISCONNECTED) {
                
                // Helper lambda to check if zpam/zpam_maps_v IWD is latest, allowing suffix after version
                auto isLatestZpamIwd = [&](const char* prefix) -> bool {
                    int prefixLen = (int)strlen(prefix);
                    if (strnicmp(result[i], prefix, prefixLen) != 0) return false;
                    // Find version number after prefix
                    const char* verStart = result[i] + prefixLen;
                    // Check if first char is digit
                    if (!isdigit((unsigned char)verStart[0])) return false;
                    int currentVersion = atoi(verStart);
                    for (int j = 0; j < *numFiles; j++) {
                        if (j == i) continue;
                        if (strnicmp(result[j], prefix, prefixLen) == 0) {
                            const char* otherVerStart = result[j] + prefixLen;
                            int otherVersion = atoi(otherVerStart);
                            // If otherVersion > currentVersion, it's newer
                            if (otherVersion > currentVersion) {
                                return false;
                            }
                        }
                    }
                    return true;
                };

                // Allow latest "zpam[...].iwd" file
                if (isLatestZpamIwd("zpam")) {
                    Com_Printf("Allowed latest ZPAM IWD file: %s\n", result[i]);
                    isOriginalFile = true;
                }

                // Allow latest "zpam_maps_vX[...].iwd" file
                if (isLatestZpamIwd("zpam_maps_v")) {
                    Com_Printf("Allowed latest ZPAM_MAPS IWD file: %s\n", result[i]);
                    isOriginalFile = true;
                }
                
            }

            if (isOriginalFile) {
                if (writeIndex != i) {
                    char* temp = result[writeIndex];
                    result[writeIndex] = result[i];
                    result[i] = temp;
                }
                writeIndex++;
            } else {
                Com_Printf("Filtered out IWD file: %s\n", result[i]);
                //Z_FreeInternal(result[i]); // free memory allocated by original function
            }
        }
        *numFiles = writeIndex;
        //result[i] = NULL; // terminate the list
    }


    return result;
}


void CL_InitDownloads() {
    // Since we are filtering loaded IWD files for demo, game then thought some required IWD files were missing
    // Fix that by calling CL_DownloadsComplete directly
    if (demo_isPlaying) {
        // Call CL_DownloadsComplete
        ASM_CALL(RETURN_VOID, 0x0040dc20); 
        return;
    }

    // Call the original CL_InitDownloads function
    ASM_CALL(RETURN_VOID, 0x0040df10);
}



// Called on /disconnect
int CL_Disconnect_CMD_disconnect() {
    int cs = clientState;

    int ret;
    ASM_CALL(RETURN(ret), 0x0040cd10);

    // If player was connected to server, reset fs_game to main and restart FS
    if (cs >= CLIENT_STATE_CONNECTED)
    {
        Dvar_SetString(Dvar_GetDvarByName("fs_game"), "");
        FS_Restart(0);
        ASM_CALL(RETURN_VOID, 0x00415a00); // CL_ShutdownUI
    }

    return ret;
}

#endif // COD2X_WIN32



bool iwd_firstTime = true;

// Is called everytime FS system is restarted (every map change)
void FS_RegisterDvars() {
    ASM_CALL(RETURN_VOID, ADDR(0x00425110, 0x080a2c3c));

    if (iwd_firstTime) {
        iwd_firstTime = false;

        // Cleanup old CoD2x IWD files that are no longer needed
        iwd_cleanupCoD2xIwdFiles();

        // Extract embedded iw_CoD2x_01.iwd file into main folder
        iwd_extractIwdFileToMain("iw_CoD2x_01", EMBEDDED_FILE_SYMBOLS(iw_CoD2x_01_iwd));
    }
}

/** Called every frame on frame start. */
void iwd_frame() {
    #if COD2X_WIN32
        static int iwd_clientStateLast = -1;

        // Player disconnects (from the server, demo, etc)
        if (clientState != iwd_clientStateLast && clientState == CLIENT_STATE_DISCONNECTED) {

            // Fix crash when ui_joinGametype points to gametype that does not exists because of mod change
            Dvar_SetInt(Dvar_GetDvarByName("ui_joinGametype"), 0);
        }

        iwd_clientStateLast = clientState;
    #endif
}


/** Called before the entry point is called. Used to patch the memory. */
void iwd_patch() {
    patch_call(ADDR(0x00425284, 0x080a2dfc), (unsigned int)&FS_RegisterDvars);

    patch_call(ADDR(0x0043bb47, 0x080654fa), (unsigned int)&WL(FS_iwIwd_Win32, FS_iwIwd));  
    patch_call(ADDR(0x00455455, 0x0808ff2c), (unsigned int)&WL(FS_iwIwd_Win32, FS_iwIwd)); // 
    patch_call(ADDR(0x0045ab96, 0x08094fce), (unsigned int)&WL(FS_iwIwd_Win32, FS_iwIwd)); // SVC_Status
    patch_call(ADDR(0x0045b2e5, 0x08095822), (unsigned int)&WL(FS_iwIwd_Win32, FS_iwIwd)); // SVC_Info

    #if COD2X_WIN32
    patch_call(0x00424869, (unsigned int)Sys_ListFiles);
    patch_call(0x00413e4e, (unsigned int)CL_InitDownloads);
    patch_call(0x0040d5fb, (unsigned int)CL_Disconnect_CMD_disconnect);
    #endif
}