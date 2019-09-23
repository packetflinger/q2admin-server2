#include "server.h"

void LOG_Frag_f(q2_server_t *srv)
{

}

void LOG_Chat_f(q2_server_t *srv)
{
	char *sql, *string;
	MYSQL_STMT *st;
	MYSQL_BIND params[2];
	int status, value;
	uint8_t cl;
	char *str;

	cl = MSG_ReadByte();
	str = MSG_ReadString();

	sql = "INSERT INTO chat (server, chat_date, message) VALUES (?, NOW(), ?)";

	st = mysql_stmt_init(srv->db);

	if (mysql_stmt_prepare(st, sql, strlen(sql))) {
		printf("%s\n", mysql_stmt_error(st));
	}

	memset(&params, 0x0, sizeof(params));

	params[0].buffer_type = MYSQL_TYPE_LONG;
	params[0].buffer = (char *) &srv->id;

	params[1].buffer_type = MYSQL_TYPE_STRING;
	params[1].buffer = str;
	params[1].buffer_length = strlen(str);

	if (mysql_stmt_bind_param(st, params)) {
		printf("%s\n", mysql_stmt_error(st));
	}

	if (mysql_stmt_execute(st)) {
		printf("%s\n", mysql_stmt_error(st));
	}

	mysql_stmt_close(st);
}
