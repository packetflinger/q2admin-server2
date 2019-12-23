#include "server.h"

/**
 * Looks up available servers in the database and formats the output
 * to send to client
 *
 */
uint16_t TP_GetServers(q2_server_t srv, char *buffer)
{
	static MYSQL_RES *res;
	static MYSQL_ROW r;

	buffer[0] = 0;
	strcat(buffer, "Available servers:\n");

	if (mysql_query(db, "SELECT id, teleportname, maxclients, map FROM server WHERE enabled = 1 AND authorized = 1")) {
		printf("%s\n", mysql_error(db));
		return 0;
	}

	res = mysql_store_result(db);
	while ((r = mysql_fetch_row(res))) {
		strcat(buffer, va("%s (%s) x/%d - *playernames here*\n", r[1], r[3], atoi(r[2])));
	}

	return strlen(buffer);
}
