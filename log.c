#include "server.h"

void LOG_Frag_f(q2_server_t *srv)
{

}

void LOG_Chat_f(q2_server_t *srv)
{
	char *sql;
	MYSQL_STMT *st;
	MYSQL_BIND params[4];
	uint8_t cl, pl;
	char *str;

	cl = MSG_ReadByte();
	str = MSG_ReadString();

	sql = "INSERT INTO chat (server, client_num, player_id, chat_date, message) VALUES (?, ?, ?, NOW(), ?)";

	st = mysql_stmt_init(srv->db);

	if (mysql_stmt_prepare(st, sql, strlen(sql))) {
		printf("%s\n", mysql_stmt_error(st));
	}

	pl = 0;	// for now player_id is just 0

	memset(&params, 0x0, sizeof(params));

	params[0].buffer_type = MYSQL_TYPE_LONG;
	params[0].buffer = (char *) &srv->id;

	params[1].buffer_type = MYSQL_TYPE_LONG;
	params[1].buffer = (char *) &cl;

	params[2].buffer_type = MYSQL_TYPE_LONG;
	params[2].buffer = (char *) &pl;

	params[3].buffer_type = MYSQL_TYPE_STRING;
	params[3].buffer = str;
	params[3].buffer_length = strlen(str);

	if (mysql_stmt_bind_param(st, params)) {
		printf("%s\n", mysql_stmt_error(st));
	}

	if (mysql_stmt_execute(st)) {
		printf("%s\n", mysql_stmt_error(st));
	}

	mysql_stmt_close(st);
}
