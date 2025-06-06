#include "master_server.h"

#include <iostream>
#include <string>

#include "shared.h"
#include "../shared/cod2_dvars.h"
#include "../shared/cod2_net.h"
#include "../shared/cod2_shared.h"
#include "../shared/cod2_cmd.h"


#define cls_masterServersCount  (*(int*)0x0068ef48)
#define cls_masterServers       (*(void**)0x0068ef6e)
#define cls_waitingForResponse  (*(int*)0x0068ef44)
#define cls_pingUpdateSource    (*(int*)0x009663d0)


/**
 * Comamnd /globalservers that is being executed when asking for servers in the server browser.
 */
void CMD_GlobalServers() {

    // Some operation with master servers
    if (cls_masterServersCount > 0)
    {
        uint8_t* serverPtr = (uint8_t*)&cls_masterServers;
        for (int i = 0; i < cls_masterServersCount; ++i)
        {
            if (*serverPtr >= 0xff)
                *serverPtr = 0xff;
            else
                (*serverPtr)++;
            serverPtr += 0x94;
        }
    }

    cls_waitingForResponse = 1;
    cls_pingUpdateSource = 1;

    // Original Activision master server
    netaddr_s to;
    if (NET_StringToAdr(SERVER_ACTIVISION_MASTER_URI, &to)) {
        Com_Printf("Requesting servers from " SERVER_ACTIVISION_MASTER_URI "...\n");
    }
	to.port = BigShort(SERVER_ACTIVISION_MASTER_PORT);

    // CoD2x master server
    netaddr_s to_cod2x;
    if (NET_StringToAdr(SERVER_MASTER_URI, &to_cod2x)) {
        Com_Printf("Requesting servers from " SERVER_MASTER_URI "...\n");
    }
    to_cod2x.port = BigShort(SERVER_MASTER_PORT);

    // Original master server - get 1.3 and 1.4 servers
    if (to.type > NA_BAD) {
        NET_OutOfBandPrint(NS_SERVER, to, "getservers 118 full empty");
        NET_OutOfBandPrint(NS_SERVER, to, "getservers 120 full empty");
    } else {
        Com_Printf("Failed to resolve master server address: %s\n", SERVER_ACTIVISION_MASTER_URI);
    }

    // CoD2x master server - get 1.3 and 1.4 servers
    if (to_cod2x.type > NA_BAD) {
        NET_OutOfBandPrint(NS_SERVER, to_cod2x, "getservers 118 full empty");
        NET_OutOfBandPrint(NS_SERVER, to_cod2x, "getservers 120 full empty");
    } else {
        Com_Printf("Failed to resolve master server address: %s\n", SERVER_MASTER_URI);
    }
}

/**
 * Handles the server info packet received from individual game servers
 */
void CL_ServerInfoPacket(netaddr_s from, msg_t *msg, int time) {

    int readcount = msg->readcount;
    char* str = MSG_ReadString(msg);
    msg->readcount = readcount; // Restore readcount after reading the string for the original function

    int protocol = atol(Info_ValueForKey(str, "protocol"));

    // Allow servers with both protocol 118 and 120
    if (protocol != 120 && protocol != 118) {
        Com_DPrintf("Different protocol info packet: %s\n", str);
        return;
    }

    ASM_CALL(RETURN_VOID, 0x004b34e0, 7, PUSH_STRUCT(from, 5), PUSH(msg), PUSH(time));
}


/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void master_server_init() {

}


/** Called before the entry point is called. Used to patch the memory. */
void master_server_patch() {
    patch_call(0x0040ecc6, (unsigned int)CL_ServerInfoPacket); // in CL_ConnectionlessPacket, "infoResponse" answer from serveres

    // Disable check for protocol 118, as its handled in our custom function CL_ServerInfoPacket
    patch_byte(0x004b355a, 0xeb); // Jump to the end of the function, skipping the protocol check (0x7419 je => 0xeb19 jmp)

    // Patch command "globalservers" to use our function
    patch_int32(0x004114ff + 1, (int)CMD_GlobalServers); 
}