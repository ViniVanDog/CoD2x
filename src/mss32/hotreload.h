#ifndef HOTRELOAD_H
#define HOTRELOAD_H

#include <windows.h>

extern bool volatile hotreload_requested;

void hotreload_loadDLL();
void hotreload_watch_dll();
void hotreload_unload();
void hotreload_init();

#endif