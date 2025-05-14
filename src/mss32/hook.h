#ifndef HOOK_H
#define HOOK_H

#include <windows.h>

extern unsigned int gfx_module_addr;

int __cdecl hook_gfxDll();
void __cdecl hook_Com_Init(const char* cmdline);
bool hook_patch();

#endif