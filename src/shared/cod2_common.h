#ifndef COD2_COMMON_H
#define COD2_COMMON_H

#include "shared.h"
#include <stdio.h>

enum errorParm_e
{
    // Show message box, close window, show console window
	ERR_FATAL = 0x0,
    // Show ingame error
	ERR_DROP = 0x1,
	ERR_SERVERDISCONNECT = 0x2,
	ERR_DISCONNECT = 0x3,
	ERR_SCRIPT = 0x4,
	ERR_SCRIPT_DROP = 0x5,
	ERR_LOCALIZATION = 0x6,
	ERR_MAPLOADERRORSUMMARY = 0x7,
};

/**
 * Show error with level, for example ERR_LEVEL
 * Example:
 *  Com_Error(ERR_DROP, "Error message %s %i", "test", 1337);
 */
inline void Com_Error(enum errorParm_e type, const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, val);
    va_end(val);
    ((void(__cdecl*)(int, const char*, const char*))(ADDR(0x004324c0, 0x08061124)))(type, "%s", buffer);
}

/**
 * Print message to console
 * Example:
 *  Com_Printf("Hello %s %i\n", "world", 1337);
 */
inline int Com_Printf(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, val);
    va_end(val);
    return ((int(__cdecl*)(const char*, const char*))(ADDR(0x00431ee0, 0x08060dea)))("%s", buffer);
}

/**
 * Print message to console when developer mode is enabled
 * Example:
 *  Com_DPrintf("Hello %s %i\n", "world", 1337);
 */
inline int Com_DPrintf(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, val);
    va_end(val);
    return ((int(__cdecl*)(const char*, const char*))(ADDR(0x00431f30, 0x08060e3a)))("%s", buffer);
}


#endif