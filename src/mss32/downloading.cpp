#include "downloading.h"

#include "shared.h"


int EventListTimerHandler (void * timer, void * param) {

    // CoD2x: Fix crash at 0x0054e658 when trying to access param->timeouts
    if (param == NULL) {
        Com_Printf("EventListTimerHandler: param is NULL\n");
        return -1; // HT_ERROR
    }

    int ret;
    ASM_CALL(RETURN(ret), 0x0054e650, 2, PUSH(timer), PUSH(param));

    return ret;
}


/** Called before the entry point to patch memory for downloading features. */
void downloading_patch() {
    patch_int32(0x0054e90c + 1, (unsigned int)EventListTimerHandler); // push EventListTimerHandler
}