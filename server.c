
#include "server.h"


/**
 * Loads config from file. Uses glib2's ini parsing stuff
 */
void LoadConfig(char *filename)
{
	// problems reading file, just use default port
	if (access(filename, F_OK) == -1) {
		config.port = PORT;
		return;
	}

	printf("Loading config from %s\n", filename);

	g_autoptr(GError) error = NULL;
	g_autoptr(GKeyFile) key_file = g_key_file_new ();

	if (!g_key_file_load_from_file(key_file, filename, G_KEY_FILE_NONE, &error)) {
		if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			g_warning ("Error loading key file: %s", error->message);
		}
	    return;
	}

	gchar *val;
	val = g_key_file_get_string (key_file, "database", "host", &error);
	strncpy(config.db_host, val, sizeof(config.db_host));

	val = g_key_file_get_string (key_file, "database", "user", &error);
	strncpy(config.db_user, val, sizeof(config.db_user));

	val = g_key_file_get_string (key_file, "database", "password", &error);
	strncpy(config.db_pass, val, sizeof(config.db_pass));

	val = g_key_file_get_string (key_file, "database", "db", &error);
	strncpy(config.db_schema, val, sizeof(config.db_schema));

	gint val2 = g_key_file_get_integer(key_file, "server", "port", &error);
	config.port = (uint16_t) val2;

	g_free(val);
}

/**
 * MySQL test function
 */
char *getNow(MYSQL *m)
{
	static MYSQL_RES *res;
	static MYSQL_ROW r;

	if (mysql_query(m, "SELECT NOW() AS now")) {
		fprintf(stderr, "%s\n", mysql_error(m));
		mysql_close(m);
		exit(1);
	}

	res = mysql_store_result(m);

	while ((r = mysql_fetch_row(res))) {
		return r[0];
	}

	return "";
}

/**
 * Free memory used by the servers linked-list. Called when list is updated
 */
void FreeServers(q2_server_t *listhead)
{
	q2_server_t *tmp;

	while (listhead != NULL) {
		tmp = listhead;
		listhead = (q2_server_t *) listhead->next;
		free(tmp);
	}
}


/**
 * Fetch active servers from the database and load into a list
 */
bool LoadServers()
{
	static MYSQL_RES *res;
	static MYSQL_ROW r;
	q2_server_t *temp;
	uint32_t err;
	struct addrinfo hints;
	char strport[7];

	printf("  * Loading servers from database *\n");

	if (mysql_query(db, "SELECT *, inet_ntoa(addr) AS ip FROM server WHERE enabled = 1")) {
		printf("%s\n", mysql_error(db));
		return false;
	}

	res = mysql_store_result(db);

	while ((r = mysql_fetch_row(res))) {

		temp = malloc(sizeof(q2_server_t));
		memset(temp, 0x0, sizeof(q2_server_t));

		temp->id = atoi(r[0]);
		temp->key = atoi(r[2]);
		temp->port = atoi(r[4]);
		strncpy(temp->password, r[5], sizeof(temp->password));
		temp->maxclients = atoi(r[6]);
		temp->enabled = atoi(r[7]);
		temp->authorized = atoi(r[8]);
		temp->flags = atoi(r[10]);
		strncpy(temp->name, r[11], sizeof(temp->name));
		strncpy(temp->teleportname, r[12], sizeof(temp->teleportname));
		temp->lastcontact = atoi(r[15]);
		strncpy(temp->map, r[16], sizeof(temp->map));
		strncpy(temp->ip, r[17], sizeof(temp->ip));

		memset(&hints, 0, sizeof(hints));
		sprintf(strport, "%d", temp->port);

		err = getaddrinfo(temp->ip, strport, &hints, &temp->addr);
		temp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

		temp->db = mysql_init(NULL);
		if (mysql_real_connect(temp->db, config.db_host, config.db_user, config.db_pass, config.db_schema, 0, NULL, 0) == NULL) {
			printf("%s database connection error: %s\n", temp->teleportname, mysql_error(temp->db));
			mysql_close(temp->db);

			continue;
		}

		printf("   * %s (%s) %s\n", temp->name, temp->teleportname, temp->ip);

		// head
		if (server_list == NULL) {
			server_list = temp;
			server_list->head = temp;
		} else {
			temp->head = server_list->head;
			server_list->next = temp;
			server_list = server_list->next;
		}
	}

	return true;
}

void *ProcessQueue(void *arg)
{
	threadrunning = true;
	while (!g_queue_is_empty(queue)) {

	}

	threadrunning = false;
	return NULL;
}

/**
 * Add to the end of the message queue
 */
void push(struct message *queue, const char *buf, size_t len)
{

}

/**
 * Get the server entry based on the supplied key
 */
q2_server_t *find_server(uint32_t key)
{
	q2_server_t *tmp;
	tmp = server_list->head;

	while (tmp) {
		if (tmp->key == key) {
			return tmp;
		}
		tmp = tmp->next;
	}

	return NULL;
}

/**
 * Get the server entry based on the supplied key
 */
q2_server_t *find_server_by_name(const char *name)
{
	q2_server_t *tmp;
	tmp = server_list->head;

	while (tmp) {
		if (!strcmp(name, tmp->teleportname)) {
			return tmp;
		}
		tmp = tmp->next;
	}

	return NULL;
}

/**
 * Called for every datagram we receive.
 */
void ProcessServerMessage()
{
	uint32_t key;
	q2_server_t *server;
	byte cmd;
	uint32_t version;

	key = MSG_ReadLong();
	server = find_server(key);

	// not a valid server, just move on with life
	if (!server) {
		return;
	}

	cmd = MSG_ReadByte();

	switch (cmd) {
	case CMD_REGISTER:
		printf("REG\n");
		CMD_Register_f(server);
		break;
	case CMD_QUIT:
		printf("QUIT\n");
		break;
	case CMD_PRINT:
		printf("PRINT\n");
		LOG_Chat_f(server);
		break;
	case CMD_FRAG:
		printf("FRAG\n");
		break;
	case CMD_TELEPORT:
		CMD_Teleport_f(server);
		break;
	}
}

void SendRCON(q2_server_t *srv, const char *fmt, ...) {
	uint16_t i;
	size_t len;
	char str[MAX_STRING_CHARS];
	va_list argptr;

	char buf[MAX_STRING_CHARS + 9];

	va_start(argptr, fmt);
    len = vsnprintf(str, sizeof(str), fmt, argptr);
	va_end(argptr);

	len = strlen(str);

	buf[0] = 0;

	strcpy(buf, "\xff\xff\xff\xffrcon\x20");
	strcat(buf, srv->password);
	strcat(buf, "\x20");
	strcat(buf, str);
	strcat(buf, "\x00");

	int r = sendto(
		srv->sockfd,
		buf,
		strlen(buf),
		MSG_DONTWAIT,
		srv->addr->ai_addr,
		srv->addr->ai_addrlen
	);

	memset(&srv->msg, 0, sizeof(msg_buffer_t));
	buf[0] = 0;
}

/**
 * Entry point
 */
int main(int argc, char **argv)
{
	struct sockaddr_in servaddr, cliaddr;
	pthread_t threadid;
	threadrunning = false;

	// load the config
	LoadConfig(CONFIGFILE);

	// connect to the database
	db = mysql_init(NULL);
	if (mysql_real_connect(db, config.db_host, config.db_user, config.db_pass, config.db_schema, 0, NULL, 0) == NULL) {
		fprintf(stderr, "%s\n", mysql_error(db));
		mysql_close(db);
		exit(1);
	}

	queue = g_queue_new();

	server_list = 0;

	LoadServers();

	printf("Listening for Quake 2 traffic on udp/%d\n", config.port);

	// Creating socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));
	
	// Filling server information 
	servaddr.sin_family = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(config.port);
	
	// Bind the socket with the server address 
	if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	int len, n;

	pthread_create(&threadid, NULL, ProcessQueue, NULL);

	while (1) {

		n = recvfrom(sockfd, (char *) msg.data, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		msg.data[n] = 0;
		msg.index = 0;

		// decrypt it here

		ProcessServerMessage();
	}

	return EXIT_SUCCESS;
}

