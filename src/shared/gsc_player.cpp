#include "gsc_player.h"

#include "shared.h"
#include "cod2_common.h"
#include "cod2_script.h"
#include "cod2_server.h"

/** Get the 32 character long HWID2 of a player */
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

/**
 * Get the CDKey hash of a player.
 * The CDKey hash is a 32 character long string sent by the client during connection.
 * It is the MD5 hash of the CDKey.
 * If the server is not cracked, this hash sent to the authorization server to validate the CDKey.
 * This hash can not be trusted as the client can send any value.
 * User can change the hash by setting different CDKey in the game options.
 * Call getAuthorizationStatus() to check if the CDKey is valid.
 */ 
void gsc_player_playerGetCDKeyHash(scr_entref_t ref) {
	int id = ref.entnum;

	if ( id >= MAX_CLIENTS )
	{
		Scr_Error(va("entity %d is not a player", id));
		Scr_AddUndefined();
		return;
	}

	client_t *client = &svs_clients[id];

	Scr_AddString(client->clientPBguid);
}


/**
 * Get the authorization status of a player's CDKey.
 * The status is a string sent by the authorization server during client connection.
 * Is "KEY_IS_GOOD" if the CDKey is valid.
 * Is "INVALID_CDKEY" if the player is using invalid CDKey, or key is in use by another player.
 * Is "CLIENT_UNKNOWN_TO_AUTH" if the authorization server does not know the client (the client can not reach the auth server).
 * Is "BANNED_CDKEY" if the CDKey is banned.
 * If the server is cracked (sv_cracked 1), the status can be any string, as the server will accept any CDKey.
 * Is empty if the player is connected via LAN network.
 * Is empty if the game is running locally from listen server.
 */
void gsc_player_playerGetAuthorizationStatus(scr_entref_t ref) {
	int id = ref.entnum;

	if ( id >= MAX_CLIENTS )
	{
		Scr_Error(va("entity %d is not a player", id));
		Scr_AddUndefined();
		return;
	}

	client_t *client = &svs_clients[id];

	// In CoD2x, we store the authorization status in PBguid field of the challenge structure
	Scr_AddString(client->PBguid);
}