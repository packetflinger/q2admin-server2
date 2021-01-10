#include "server.h"


/**
 *
 */
void ParseMessage(q2_server_t *q2, msg_buffer_t *msg)
{
    uint8_t cmd;
    msg_buffer_t e;

    if (q2->connection.encrypted) {
        memset(&e, 0, sizeof(msg_buffer_t));
        e.length = SymmetricDecrypt(q2, e.data, msg->data, msg->length);
        memset(msg, 0, sizeof(msg_buffer_t));
        memcpy(msg->data, e.data, e.length);
        msg->length = e.length;
    }

    hexDump("New Message", msg->data, msg->length);

    // keep parsing msgs while data is in the buffer
    while (msg->index < msg->length) {
        cmd = MSG_ReadByte(msg);

        switch(cmd) {
        case CMD_AUTH:
            ParseAuth(q2, msg);
            break;
        case CMD_QUIT:
            CloseConnection(q2);
            break;
        case CMD_PING:
            Pong(q2);
            break;
        case CMD_PRINT:
            ParsePrint(q2, msg);
            break;
        case CMD_COMMAND:
            ParseCommand(q2, msg);
            break;
        case CMD_CONNECT:
            ParsePlayerConnect(q2, msg);
            break;
        case CMD_PLAYERUPDATE:
            ParsePlayerUpdate(q2, msg);
            break;
        case CMD_DISCONNECT:
            ParsePlayerDisconnect(q2, msg);
            break;
        case CMD_PLAYERLIST:
            ParsePlayerList(q2, msg);
            break;
        case CMD_MAP:
            ParseMap(q2, msg);
            break;
        //default:
            //printf("cmd: %d\n", cmd);
        }
    }
}

/**
 * Parse the first message sent from the client
 */
void ParseHello(hello_t *h, msg_buffer_t *in)
{
	h->key = MSG_ReadLong(in);
	h->version = MSG_ReadLong(in);
	h->port = MSG_ReadShort(in);
	h->max_clients = MSG_ReadByte(in);
	h->encrypted = MSG_ReadByte(in);
	MSG_ReadData(in, h->challenge, CHALLENGE_LEN);
}

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

/**
 * A client issued an invite command
 */
void ParseInvite(q2_server_t *srv, msg_buffer_t *in)
{
	q2_server_t *s;
	uint8_t client_id;
	char *text;
	char message[MAX_STRING_CHARS];

	client_id = MSG_ReadByte(in);
	text = MSG_ReadString(in);

	// handle flood detection here

	if (text[0]) {
		strncpy(message,
			va("%s invites you to play on %s: %s",
					srv->players[client_id].name,
					srv->name,
					text
			),
			sizeof(message)
		);
	} else {
		strncpy(message,
			va("You are cordially invited to join %s playing on %s",
					srv->players[client_id].name,
					srv->name
			),
			sizeof(message)
		);
	}

	FOR_EACH_SERVER(s) {
		if (!s->connected) {
			continue;
		}

		MSG_WriteByte(SCMD_SAYALL, &s->msg);
		MSG_WriteString(message, &s->msg);
		SendBuffer(s);
	}
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

	printf("%d disconnected from %s\n", client_id, srv->name);

	memset(&srv->players[client_id], 0, sizeof(q2_player_t));
	srv->playercount--;
}

void ParseMap(q2_server_t *srv, msg_buffer_t *in)
{
	char *map;

	map = MSG_ReadString(in);
	strncpy(srv->map, map, sizeof(srv->map));
}

void ParsePlayerList(q2_server_t *srv, msg_buffer_t *in)
{
	uint8_t i, client_id;
	char *ui;
	q2_player_t *p;

	srv->playercount = MSG_ReadByte(in);

	for (i=0; i<srv->playercount; i++) {
		p = &srv->players[i];
		i = MSG_ReadByte(in);
		ui = MSG_ReadString(in);

		memset(p, 0, sizeof(q2_player_t));

		p->client_id = i;
		strncpy(p->userinfo, ui, sizeof(p->userinfo));
		strncpy(p->name, Info_ValueForKey(ui, "name"), sizeof(p->name));

		printf("Found %s\n", p->name);
	}
}

void ParseAuth(q2_server_t *q2, msg_buffer_t *in)
{
    if (!VerifyClientChallenge(q2, in)) {
        MSG_WriteByte(SCMD_ERROR, &q2->msg);
        MSG_WriteByte(-1, &q2->msg);
        MSG_WriteByte(ERR_UNAUTHORIZED, &q2->msg);
        MSG_WriteString("Client authentication failed", &q2->msg);
        SendBuffer(q2);

        //ERR_CloseConnection(q2);
    }

    printf("%s is trusted\n", q2->name);
    MSG_WriteByte(SCMD_TRUSTED, &q2->msg);
    SendBuffer(q2);
}
