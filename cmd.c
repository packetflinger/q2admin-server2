#include "server.h"


void CMD_Register_f(q2_server_t *srv)
{
	uint32_t version;

	version = MSG_ReadLong();

	// version too old, inform server, remove entry
	if (VER_REQ > version) {
		return;
	}
}

/**
 * Called when a player uses the teleport command.
 * if no args, list servers, else stuff a connect
 */
void CMD_Teleport_f(q2_server_t *srv)
{
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
		SendRCON(srv, "say player %d looking for servers", player);
	}
}


/**
 * Send an invite to all servers
 */
void CMD_Invite_f(q2_server_t *srv)
{
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
}
