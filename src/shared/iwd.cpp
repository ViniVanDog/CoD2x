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
}
