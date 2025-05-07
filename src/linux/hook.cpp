#include "hook.h"

#include <sys/mman.h> // mprotect

#include "shared.h"
#include "../shared/common.h"
#include "../shared/server.h"
#include "../shared/game.h"
#include "updater.h"


/**
 * Com_Init
 * Is called in main function when the game is started. Is called only once on game start.
 */
void __cdecl hook_Com_Init(char* cmdline) {

    Com_Printf("Command line: '%s'\n", cmdline);
    
    server_init();

    // Call the original function
	((void (__cdecl *)(char*))0x080620c0)(cmdline);

    common_init();
    updater_init();
    game_init();
}


/**
 * Com_Frame
 * Is called in the main loop every frame.
 */
void __cdecl hook_Com_Frame() {

    // Call the original function
	((void (__cdecl *)())0x080626f4)();
}


/**
 * Patch the cod2_lnxded.so executable
 */
bool hook_patch() {

    // allow to write in executable memory
	mprotect((void *)0x08048000, 0x134000, PROT_READ | PROT_WRITE | PROT_EXEC);


    // Patch Com_Init
    patch_call(0x0806233d, (unsigned int)hook_Com_Init);

    // Patch Com_Frame
    patch_call(0x0806281a, (unsigned int)hook_Com_Frame);


    common_patch();
    server_patch();
    game_patch();
    updater_patch();

    return true;
}