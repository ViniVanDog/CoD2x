#ifndef MAIN_H
#define MAIN_H

#include <windows.h>

extern HMODULE EXE_HMODULE;
extern char EXE_PATH[MAX_PATH];
extern char EXE_DIRECTORY_PATH[MAX_PATH];
extern char DLL_PATH[MAX_PATH];
extern bool DLL_HOTRELOAD;

// DLL entry point declaration
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);

#endif