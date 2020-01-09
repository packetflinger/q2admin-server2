#include "server.h"


/**
 * Parse in incoming PRINT message.
 * 1 byte for the print level, and
 * a null-terminated string
 */
void ParsePrint(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t level;
	char *string;

	level = MSG_ReadByte(in);
	string = MSG_ReadString(in);

	// obituary, parse for means of death
	if (level == PRINT_MEDIUM) {

	}

	if (level == PRINT_CHAT) {
		// log chat
	}
}

/**
 * Parse a command.
 * This would be player-initiated commands like to teleport or invite
 */
void ParseCommand(q2_server_t *srv, msg_buffer_t *in)
{
	switch(MSG_ReadByte(in))
	{
	case CMD_COMMAND_TELEPORT:
		ParseTeleport(srv, in);
		break;
	case CMD_COMMAND_INVITE:
		ParseInvite(srv, in);
		break;
	case CMD_COMMAND_WHOIS:
		ParseWhois(srv, in);
		break;
	}
}

void ParseTeleport(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t client_id;
	char *location;
	char *reply;

	client_id = MSG_ReadByte(in);
	location = MSG_ReadString(in);

	// do stuff
	reply = va("client %d just used teleport command\n", client_id);

	MSG_WriteByte(SCMD_SAYCLIENT, &srv->msg);
	MSG_WriteByte(client_id, &srv->msg);
	MSG_WriteByte(PRINT_HIGH, &srv->msg);
	MSG_WriteString(reply, &srv->msg);

	SendBuffer(srv);
}

void ParseInvite(q2_server_t *srv, msg_buffer_t *in)
{

}

void ParseWhois(q2_server_t *srv, msg_buffer_t *in)
{

}

void ParsePlayerConnect(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t client_id;
	char *userinfo;
	char *name;
	q2_player_t *p;

	client_id = MSG_ReadByte(in);
	userinfo = MSG_ReadString(in);
	name = Info_ValueForKey(userinfo, "name");

	printf("%s just connected\n", name);

	memset(&srv->players[client_id], 0, sizeof(q2_player_t));
	p = &srv->players[client_id];
	p->client_id = client_id;
	strncpy(p->name, name, sizeof(p->name));
	strncpy(p->userinfo, userinfo, sizeof(p->userinfo));

	srv->playercount++;
}

void ParsePlayerUpdate(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t client_id;
	char *userinfo;
	char *name;
	q2_player_t *p;

	client_id = MSG_ReadByte(in);
	userinfo = MSG_ReadString(in);
	name = Info_ValueForKey(userinfo, "name");

	printf("%s updated\n", name);

	memset(&srv->players[client_id], 0, sizeof(q2_player_t));
	p = &srv->players[client_id];
	p->client_id = client_id;
	strncpy(p->name, name, sizeof(p->name));
	strncpy(p->userinfo, userinfo, sizeof(p->userinfo));
}

void ParsePlayerDisconnect(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t client_id = MSG_ReadByte(in);

	printf("%d just disconnected\n", client_id);

	memset(&srv->players[client_id], 0, sizeof(q2_player_t));
	srv->playercount--;
}

void ParseMap(q2_server_t *srv, msg_buffer_t *in)
{
	char *map;

	map = MSG_ReadString(in);
	strncpy(srv->map, map, sizeof(srv->map));
}
