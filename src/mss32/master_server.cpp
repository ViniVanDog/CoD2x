#include "master_server.h"

#include <iostream>
#include <string>

#include "shared.h"
#include "../shared/cod2_dvars.h"

dvar_t *cl_masterServer;
dvar_t *cl_masterPort;

/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void master_server_init() {

    for (int i = 0; i <= 1; i++)
    {
        dvarFlags_e flags = i == 0 ? 
            (dvarFlags_e)(DVAR_LATCH | DVAR_CHANGEABLE_RESET) : // allow the value to be changed via cmd when starting the game
            (dvarFlags_e)(DVAR_ROM | DVAR_CHANGEABLE_RESET);    // then make it read-only to avoid changes

        cl_masterServer = Dvar_RegisterString("cl_masterServer", SERVER_ACTIVISION_MASTER_URI, flags);
        cl_masterPort = Dvar_RegisterInt("cl_masterPort", SERVER_ACTIVISION_MASTER_PORT, 1, 65535, flags);
    }

    // Server used to request the list of servers
    // After the cvars are loaded, change the master server address
    patch_string_ptr(0x004b3fa5 + 1, cl_masterServer->value.string);
    patch_int32(0x004b3fb4 + 1, cl_masterPort->value.integer);

    // Change the protocol used when requesting list of servers from master server
    //patch_byte(0x00539727 + 1, PROTOCOL_VERSION);
    patch_byte(0x00539727 + 1, 118);
}

