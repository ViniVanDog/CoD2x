#include "affinity.h"

#include <windows.h>

#include "../shared/cod2_common.h"
#include "../shared/cod2_dvars.h"

// Affinity: 
//   -1: all
//    0: off
// >= 1: total cores to enable (e.g. 3 enables first 3 cores)
dvar_t* com_affinity = NULL;


void affinity_set(DWORD_PTR mask) {
    if (SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        int cores = 0;
        for (DWORD_PTR m = mask; m; m >>= 1) {
            if (m & 1) ++cores;
        }
        Com_Printf("CPU Affinity: Affinity set to %d core%s (mask 0x%08X).\n", cores, cores == 1 ? "" : "s", mask);
    } else {
        Com_Printf("CPU Affinity: Error setting affinity: %lu\n", GetLastError());
    }
}

/** Called every frame on frame start. */
void affinity_frame() {

    if (com_affinity->modified) {
        // Reset the modified flag
        com_affinity->modified = false;

        // If affinity is set to 0, disable it
        if (com_affinity->value.integer == 0) {
            Com_Printf("CPU Affinity disabled.\n");
            return;
        }

        // If affinity is set to -1, set it to all available CPUs
        if (com_affinity->value.integer == -1) {
            DWORD_PTR mask = ((DWORD_PTR)1 << com_affinity->limits.integer.max) - 1;
            affinity_set(mask);
            return;
        }

        // For a positive value, enable that number of cores (e.g. 3 enables cores 0,1,2)
        DWORD_PTR mask = ((DWORD_PTR)1 << com_affinity->value.integer) - 1;
        affinity_set(mask);
    }
}

/** Called only once on game start after common initialization. Used to initialize variables, cvars, etc. */
void affinity_init() {

    // Get the system's CPU information
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    com_affinity = Dvar_RegisterInt("com_affinity", -1, -1, sysInfo.dwNumberOfProcessors, (dvarFlags_e)(DVAR_ARCHIVE | DVAR_CHANGEABLE_RESET));

    com_affinity->modified = true; // Force initial setting
}