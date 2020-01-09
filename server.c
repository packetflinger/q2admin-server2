
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
 * Variable assignment, just makes building strings easier
 */
char *va(const char *format, ...) {
	static char strings[8][MAX_STRING_CHARS];
	static uint16_t index;

	char *string = strings[index++ % 8];

	va_list args;

	va_start(args, format);
	vsnprintf(string, MAX_STRING_CHARS, format, args);
	va_end(args);

	return string;
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
		strncpy(temp->ip, r[18], sizeof(temp->ip));
		strncpy(temp->encryption_key, r[17], sizeof(temp->encryption_key));

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

		printf("   - %s (%s) %s\n", temp->name, temp->teleportname, temp->ip);

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
 * Send a reply to a q2 server PING request
 */
static void Pong(q2_server_t *srv)
{
	MSG_WriteByte(SCMD_PONG, &srv->msg);
	SendBuffer(srv);
}

/**
 * Pick out a value for a givin key in the userinfo string
 */
char *Info_ValueForKey(char *s, char *key)
{
    char pkey[512];
    static char value[2][512]; // use two buffers so compares
    // work without stomping on each other
    static int valueindex;
    char *o;

    valueindex ^= 1;
    if (*s == '\\')
        s++;
    while (1) {
        o = pkey;
        while (*s != '\\') {
            if (!*s)
                return "";
            *o++ = *s++;
        }
        *o = 0;
        s++;

        o = value[valueindex];

        while (*s != '\\' && *s) {
            if (!*s)
                return "";
            *o++ = *s++;
        }
        *o = 0;

        if (!strcmp(key, pkey))
            return value[valueindex];

        if (!*s)
            return "";
        s++;
    }
}

/**
 * XOR the current message buffer with the encryption key for this server.
 */
static void MSG_Decrypt(char* key) {
	uint16_t i;

	for (i=msg.index; i<msg.length; i++) {
		msg.data[i] ^= key[i];
	}
}


/**
 * As clients disconnect, new connections use their threadId.
 * Keeps them contiguous
 */
uint32_t find_available_thread_slot(void)
{
	uint32_t i;

	for (i = 0; i < MAX_THREADS; i++) {
		// if that thread is null, let's use the space
		if (!threads[i]) {
			return i;
		}
	}

	// getting here means we're full, no more threads allowed
	return -1;
}

/**
 * Each q2 server connection gets one of these threads
 */
void *ServerThread(void *arg)
{
	msg_buffer_t msg; // for receiving data only
	uint32_t _ret;
	uint32_t serverkey, q2arevision;
	uint16_t port;
	uint8_t maxclients;
	connection_t _q2con = *(connection_t *) arg;
	q2_server_t *q2;
	byte cmd;
	ssize_t bytessent;

	/*
	struct timeval _tv;

	_tv.tv_sec = 10;
	_tv.tv_usec = 0;
	setsockopt(_q2con.socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&_tv, sizeof(_tv));
	*/

	memset(&msg, 0, sizeof(msg_buffer_t));

	// figure out which server connection this should go to
	_ret = recv(_q2con.socket, &msg.data, sizeof(msg.data), 0);
	if (_ret < 0) {
		perror("Server thread recv");
		return NULL;
	}

	// first thing should be hello
	if (MSG_ReadByte(&msg) != CMD_HELLO) {
		printf("thread[%d] - protocol error, not a real client\n", _q2con.thread_id);
		return NULL;
	}

	serverkey = MSG_ReadLong(&msg);
	q2arevision = MSG_ReadLong(&msg);
	port = MSG_ReadShort(&msg);
	maxclients = MSG_ReadByte(&msg);

	printf("thread[%d] - server: %d\n", _q2con.thread_id, serverkey);

	// find the server entry that matches supplied serverkey
	for (server_list = server_list->head; server_list; server_list = server_list->next) {
		if (serverkey == server_list->key) {
			q2 = server_list;
			q2->socket = _q2con.socket;
			q2->connected = true;
			q2->port = port;
			q2->maxclients = maxclients;

			// q2 server won't send any more data until it receives this ACK
			MSG_WriteByte(SCMD_HELLOACK, &q2->msg);
			SendBuffer(q2);

			break;
		}
	}

	// not a valid server
	if (!q2) {
		printf("Invalid serverkey, closing connection\n");
		close(_q2con.socket);
		return NULL;
	}

	// main server connection loop
	while (true) {
		memset(&msg, 0, sizeof(msg_buffer_t));
		errno = 0;

		// read data from q2 server
		msg.length = recv(q2->socket, &msg.data, sizeof(msg.data), 0);
		if (msg.length < 0) {
			perror(va("recv (server %d)",q2->key));
		}

		if (errno) {
			printf("RECV error: %d - %s\n", errno, strerror(errno));
			CloseConnection(q2);
			break;
		}

		// keep parsing msgs while data is in the buffer
		while (msg.index < msg.length) {

			cmd = MSG_ReadByte(&msg);

			switch(cmd) {
			case CMD_QUIT:
				CloseConnection(q2);
				break;
			case CMD_PING:
				Pong(q2);
				break;
			case CMD_PRINT:
				ParsePrint(q2, &msg);
				break;
			case CMD_COMMAND:
				ParseCommand(q2, &msg);
				break;
			case CMD_CONNECT:
				ParsePlayerConnect(q2, &msg);
				break;
			case CMD_PLAYERUPDATE:
				ParsePlayerUpdate(q2, &msg);
				break;
			case CMD_DISCONNECT:
				ParsePlayerDisconnect(q2, &msg);
				break;
			default:
				printf("cmd: %d\n", cmd);
			}
		}

		if (!q2->connected) {
			break;
		}
	}

	return NULL;
}

/**
 * Send the contents of the message buffer to the q2 server
 */
void SendBuffer(q2_server_t *srv)
{
	if (!srv->connected) {
		return;
	}

	if (!srv->msg.length) {
		return;
	}

	send(srv->socket, srv->msg.data, srv->msg.length, 0);
	memset(&srv->msg, 0, sizeof(msg_buffer_t));

	return;
}

void CloseConnection(q2_server_t *srv)
{
	close(srv->socket);
	srv->connected = false;
}

/**
 * Entry point
 */
int main(int argc, char **argv)
{
	uint32_t ret;
	struct sockaddr_in servaddr, cliaddr;
	uint32_t clin_size;
	pthread_t threadid;
	connection_t conn;

	threadrunning = false;

	memset(&threads, 0, sizeof(pthread_t) * MAX_THREADS);

	// load the config
	LoadConfig(CONFIGFILE);

	// connect to the database
	db = mysql_init(NULL);
	if (mysql_real_connect(db, config.db_host, config.db_user, config.db_pass, config.db_schema, 0, NULL, 0) == NULL) {
		fprintf(stderr, "%s\n", mysql_error(db));
		mysql_close(db);
		exit(1);
	}

	server_list = 0;

	LoadServers();

	// Creating socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

	// start listening
	ret = listen(sockfd, 100);
	if (ret < 0) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	printf("Listening on tcp/%d\n", config.port);

	// listen for new connections
	while (true) {
		newsockfd = accept(sockfd, (struct sockaddr *) &cliaddr, &clin_size);
		if (newsockfd == -1) {
			perror("Accept");
			continue;
		}

		memset(&conn, 0, sizeof(connection_t));
		conn.socket = newsockfd;
		conn.thread_id = find_available_thread_slot();

		// hand connection off to new thread
		ret = pthread_create(&threads[conn.thread_id], NULL, ServerThread, &conn);
	}

	return EXIT_SUCCESS;
}

