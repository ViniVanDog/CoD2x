#include "gsc_player.h"

#include "shared.h"
#include "cod2_common.h"
#include "cod2_script.h"
#include "cod2_server.h"


void gsc_player_playerGetHWID(scr_entref_t ref) {
	int id = ref.entnum;

	if ( id >= MAX_CLIENTS )
	{
		Scr_Error(va("entity %d is not a player", id));
		Scr_AddUndefined();
		return;
	}

	client_t *client = &svs_clients[id];

	const char* HWID2 = Info_ValueForKey(client->userinfo, "cl_hwid2");

	if (HWID2 == NULL || strlen(HWID2) != 32)
	{
		Scr_Error(va("client %d has no HWID2", id)); // should never happen, as its validated on client connect
		Scr_AddUndefined();
		return;
	}

	Scr_AddString(HWID2);
}