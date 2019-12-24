#include "server.h"

/**
 * Looks up available servers in the database and formats the output
 * to send to client
 *
 */
void TP_GetServers(q2_server_t *srv, uint8_t player, char *target)
{
	q2_server_t *server;
	static MYSQL_RES *res;
	static MYSQL_ROW r;

	SendRCON(srv, "sv !say_person CL %d Available servers:\n", player);

	if (mysql_query(srv->db, "SELECT id, teleportname, maxclients, map FROM server WHERE enabled = 1 AND authorized = 1")) {
		printf("%s\n", mysql_error(srv->db));
		return;
	}

	res = mysql_store_result(srv->db);

	while ((r = mysql_fetch_row(res))) {
		server = find_server(atoi(r[0]));
		if (server) {
			/*SendRCON(
					srv,
					"sv !say_person CL %d %s (%s) %d/%d - *playerlist here*\n",
					player,
					r[1],
					r[3],
					server->playercount,
					atoi(r[2])
			);*/
			printf(
					"sv !say_person CL %d %s (%s) %d/%d - *playerlist here*\n",
					player,
					r[1],
					r[3],
					server->playercount,
					atoi(r[2])
			);
		}
	}
}
