#include "server.h"

/**
 * Q2 Server sends this when it comes online
 */
void CMD_Register_f(q2_server_t *srv)
{
	/*
	srv->version = MSG_ReadLong();

	// version too old, inform server, remove entry
	if (VER_REQ > srv->version) {
		srv->enabled = false;
		srv->active = false;
		return;
	}

	srv->port = MSG_ReadShort();
	srv->maxclients = MSG_ReadByte();
	strncpy(srv->password, MSG_ReadString(), sizeof(srv->password));
	strncpy(srv->map, MSG_ReadString(), sizeof(srv->map));

	srv->active = true;
	SendRCON(srv, CMD_ONLINE);
	*/
}

/**
 * Called when a player uses the teleport command.
 * if no args, list servers, else stuff a connect
 */
void CMD_Teleport_f(q2_server_t *srv)
{
	/*
	char buffer[2000];
	uint8_t player;
	char *target;
	q2_server_t *targetsrv;

	player = MSG_ReadByte();
	target = MSG_ReadString();

	if (strlen(target) > 0) {
		targetsrv = find_server_by_name(target);
		if (targetsrv) {
			SendRCON(srv, "sv !stuff CL %d connect %s:%d", player, targetsrv->ip, targetsrv->port);
		} else {
			SendRCON(srv, "sv !say_person CL %d Unknown server: \"%s\"", player, target);
		}
	} else {
		//SendRCON(srv, "say player %d looking for servers", player);
		TP_GetServers(srv, player, target);
		//SendRCON(srv, "sv !say_person CL %d \"%s\"", player, buffer);
	}
	*/
}


/**
 * Send an invite to all servers
 */
void CMD_Invite_f(q2_server_t *srv)
{
	/*
	uint8_t player;
	char *target;
	q2_server_t *server = srv->head;

	player = MSG_ReadByte();
	target = MSG_ReadString();

	while (server) {
		if (server->flags & RFL_INVITE) {
			SendRCON(server, "say You're Invited to %s. type \"!teleport %s\"", srv->name, srv->teleportname);
		}
	}
	*/
}

/**
 * Called when a frag happens on a server
 */
void CMD_Frag_f(q2_server_t *srv)
{
	LOG_Frag_f(srv);
}


/**
 * A player just connected to this server
 */
void CMD_PlayerConnect_f(q2_server_t *srv)
{
	/*
	uint8_t client;
	uint16_t ping;
	char *userinfo;
	q2_player_t *p;

	client = MSG_ReadByte();
	ping = MSG_ReadShort();
	userinfo = MSG_ReadString();

	srv->playercount++;

	p = &srv->players[client];
	memset(&p, 0, sizeof(q2_player_t));

	p->client_id = client;
	p->ping = ping;
	strncpy(p->userinfo, userinfo, sizeof(p->userinfo));
	strncpy(p->name, Info_ValueForKey(userinfo, "name"), sizeof(p->name));

	printf("Just added: %d:%d\n", p->client_id, p->ping);
	*/
}


/**
 * A player just disconnected
 */
void CMD_PlayerDisconnect_f(q2_server_t *srv)
{
	/*
	uint8_t client;
	q2_player_t *p;

	client = MSG_ReadByte();

	srv->playercount--;

	p = &srv->players[client];
	memset(&p, 0, sizeof(q2_player_t));
	*/
}
