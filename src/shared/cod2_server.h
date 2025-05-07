#ifndef COD2_SERVER_H
#define COD2_SERVER_H

#include "shared.h"
#include "assembly.h"
#include "cod2_shared.h"
#include "cod2_dvars.h"

#define MAX_CHALLENGES 1024

typedef struct
{
	netaddr_s adr;
	int challenge;
	int time;
	int pingTime;
	int firstTime;
	int firstPing;
	int connected;
	int guid;
	char PBguid[33];
	char clientPBguid[33];
} challenge_t;
static_assert(sizeof(challenge_t) == 0x74, "sizeof(challenge_t)");



#define SV_CMD_CAN_IGNORE 0
#define SV_CMD_RELIABLE 1

/** Sends a command string to a client. If clientNum is -1, then the command is sent to everybody */
inline void SV_GameSendServerCommand( int clientNum, int svscmd_type, const char *text ) {
    WL(
        ASM_CALL(RETURN_VOID, 0x004567b0, 0, EAX(clientNum), EDX(svscmd_type), ECX(text)),
        ASM_CALL(RETURN_VOID, 0x080917aa, 3, PUSH(clientNum), PUSH(svscmd_type), PUSH(text))
    )
};

// Set cvar on client side
inline void SV_SetClientCvar(int clientNum, const char *cvarName, const char *cvarValue) {
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, va("%c %s \"%s\"", 118, cvarName, cvarValue));
}


// Get user info
inline void SV_GetUserInfo(int index, char *buffer, int bufferSize) {
    WL(
        ASM_CALL(RETURN_VOID, 0x004580b0, 0, EAX(index), EBX(buffer), EDI(bufferSize)),
        ASM_CALL(RETURN_VOID, 0x08092C04, 3, PUSH(index), PUSH(buffer), PUSH(bufferSize))
    )
}

// Kick player from the server, returns guid
inline int SV_KickClient(void* client) {
    const char* playerName = NULL;
    int playerNameLen = 0;
    int result;
    WL(
        ASM_CALL(RETURN(result), 0x004521b0, 0, EDI(client), EAX(playerName), EBX(playerNameLen)),
        ASM_CALL(RETURN(result), 0x0808C316, 3, PUSH(client), PUSH(playerName), PUSH(playerNameLen))
    );
    return result;
}

#endif