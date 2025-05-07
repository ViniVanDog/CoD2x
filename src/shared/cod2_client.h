#ifndef COD2_CLIENT_H
#define COD2_CLIENT_H

#include "cod2_shared.h"
#include "cod2_entity.h"
#include "cod2_player.h"

#define cg_entities         (*((centity_t (*)[1024])0x015E2A80))
#define clientInfo          (*((clientInfo_t (*)[64])0x015CF994)) // client side info

// https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/q_shared.h#L1621
enum clientState_e{
	CLIENT_STATE_DISCONNECTED,    // not talking to a server
	CLIENT_STATE_CINEMATIC,       // playing a cinematic or a static pic, not connected to a server
	CLIENT_STATE_AUTHORIZING,     // not used any more, was checking cd key
	CLIENT_STATE_CONNECTING,      // sending request packets to the server
	CLIENT_STATE_CHALLENGING,     // sending challenge packets to the server
	CLIENT_STATE_CONNECTED,       // netchan_t established, getting gamestate
	CLIENT_STATE_LOADING,         // only during cgame initialization, never during main loop
	CLIENT_STATE_PRIMED,          // got gamestate, waiting for first frame
	CLIENT_STATE_ACTIVE,          // game views should be displayed       
};

inline const char* get_client_state_name(int state) {
    switch (state) {
        case CLIENT_STATE_DISCONNECTED: return "DISCONNECTED";
        case CLIENT_STATE_CINEMATIC: return "CINEMATIC";
        case CLIENT_STATE_AUTHORIZING: return "AUTHORIZING";
        case CLIENT_STATE_CONNECTING: return "CONNECTING";
        case CLIENT_STATE_CHALLENGING: return "CHALLENGING";
        case CLIENT_STATE_CONNECTED: return "CONNECTED";
        case CLIENT_STATE_LOADING: return "LOADING";
        case CLIENT_STATE_PRIMED: return "PRIMED";
        case CLIENT_STATE_ACTIVE: return "ACTIVE";
        default: return "UNKNOWN";
    }
}

typedef struct {
	entityState_t	currentState;
	entityState_t	nextState;
	int				currentValid;
	int				pad[2];
	vec3_t			lerpOrigin;
	int				pad2[11];
} centity_t; //size=548, dw=137






inline void CL_AddDebugString(float const* xyz, float const* color, float scale, char const* text, int duration) {
    ASM_CALL(RETURN_VOID, 0x00412230, 3, EBX(xyz), EDI(color), PUSH(scale), PUSH(text), PUSH(duration));
}

inline void CL_AddDebugLine(float const* xyz_start, float const* xyz_end, float const* color, int duration, int depthTest, int pernament) {
    ASM_CALL(RETURN_VOID, 0x00412300, 3, EBX(xyz_start), EDI(xyz_end), ESI(color), PUSH(depthTest), PUSH(duration), PUSH(pernament));
}

inline void CL_AddDebugCrossPoint(float const* center, float size, float const* color, int duration, int depthTest, int pernament) {
	vec3_t start, end;

	// X axis line
	VectorSet(start, -size, 0, 0);
	VectorAdd(start, center, start);
	VectorSet(end, size, 0, 0);
	VectorAdd(end, center, end);
	CL_AddDebugLine(start, end, color, duration, depthTest, pernament);

	// Y axis line
	VectorSet(start, 0, -size, 0);
	VectorAdd(start, center, start);
	VectorSet(end, 0, size, 0);
	VectorAdd(end, center, end);
	CL_AddDebugLine(start, end, color, duration, depthTest, pernament);

	// Z axis line
	VectorSet(start, 0, 0, -size);
	VectorAdd(start, center, start);
	VectorSet(end, 0, 0, size);
	VectorAdd(end, center, end);
	CL_AddDebugLine(start, end, color, duration, depthTest, pernament);
}


#endif