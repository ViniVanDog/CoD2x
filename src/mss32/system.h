#ifndef SYSTEM_H
#define SYSTEM_H

extern char SYS_WIN_BUILD[64];
extern char SYS_WIN_EDITION[64];
extern char SYS_WIN_DISPLAY_VERSION[32];
extern char SYS_WINE_VERSION[64];
extern char SYS_WINE_BUILD[64];
extern char SYS_VERSION_INFO[256]; // Version info data in format: "win-x86-22631-Professional-22H2" or "win-x86-22631-Professional-22H2-Wine-8.0-wine-8.0-3001-g39021e609a2 (Staging)"
extern char SYS_VM_NAME[64]; // Name of the virtualization platform, if detected

void system_getInfo();

#endif