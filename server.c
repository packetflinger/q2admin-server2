
#include "server.h"

LIST_DECL(q2srvlist);
LIST_DECL(connlist);

/**
 * Print out an error message and quit
 */
static void die(const char *msg) {
	printf("Error: %s\n", msg);
	exit(EXIT_FAILURE);
}

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
	val = g_key_file_get_string(key_file, "database", "host", &error);
	strncpy(config.db_host, val, sizeof(config.db_host));

	val = g_key_file_get_string(key_file, "database", "user", &error);
	strncpy(config.db_user, val, sizeof(config.db_user));

	val = g_key_file_get_string(key_file, "database", "password", &error);
	strncpy(config.db_pass, val, sizeof(config.db_pass));

	val = g_key_file_get_string(key_file, "database", "db", &error);
	strncpy(config.db_schema, val, sizeof(config.db_schema));

	gint val2 = g_key_file_get_integer(key_file, "server", "port", &error);
	config.port = (uint16_t) val2;

	gint val3 = g_key_file_get_integer(key_file, "server", "client_port", &error);
	config.client_port = (uint16_t) val3;

	gint val4 = g_key_file_get_integer(key_file, "server", "tls_port", &error);
	config.tls_port = (uint16_t) val4;

	val = g_key_file_get_string(key_file, "crypto", "certificate", &error);
	strncpy(config.certificate, val, sizeof(config.certificate));

	val = g_key_file_get_string(key_file, "crypto", "private_key", &error);
	strncpy(config.private_key, val, sizeof(config.private_key));

	val = g_key_file_get_string(key_file, "crypto", "public_key", &error);
	strncpy(config.public_key, val, sizeof(config.public_key));

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
	q2_server_t *s;

	FOR_EACH_SERVER(s) {
		// free
	}
}


/**
 * Fetch active servers from the database and load into a list
 */
bool LoadServers()
{
	static MYSQL_RES *res;
	static MYSQL_ROW r;
	q2_server_t *temp, *server;
	uint32_t err;
	struct addrinfo hints;
	char strport[7];

	List_Init(&q2srvlist);

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
		temp->key = atoi(r[3]);
		temp->port = atoi(r[5]);
		strncpy(temp->password, r[5], sizeof(temp->password));
		temp->maxclients = atoi(r[7]);
		temp->enabled = atoi(r[8]);
		temp->authorized = atoi(r[9]);
		temp->flags = atoi(r[11]);
		strncpy(temp->name, r[12], sizeof(temp->name));
		strncpy(temp->teleportname, r[13], sizeof(temp->teleportname));
		temp->lastcontact = atoi(r[16]);
		strncpy(temp->map, r[17], sizeof(temp->map));
		strncpy(temp->public_key, r[19], sizeof(temp->public_key));
		strncpy(temp->ip, r[20], sizeof(temp->ip));

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

		List_Append(&q2srvlist, &temp->entry);

	}

	FOR_EACH_SERVER(server) {
		printf("    - %s (%s) %s:%d\n", server->name, server->teleportname, server->ip, server->port);
	}

	return true;
}

/**
 * Get the server entry based on the supplied key
 */
q2_server_t *find_server(uint32_t key)
{
	q2_server_t *s;

	FOR_EACH_SERVER(s) {
		if (s->key == key) {
			return s;
		}
	}

	return NULL;
}

/**
 * Get the server entry based on the supplied key
 */
q2_server_t *find_server_by_name(const char *name)
{
	q2_server_t *s;

	FOR_EACH_SERVER(s) {
		if (strcmp(name, s->teleportname) == 0) {
			return s;
		}
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

void SignalCatcher(int sig)
{
	q2_server_t *srv;

	// close all sockets and GTFO
	if (sig == SIGINT) {
		FOR_EACH_SERVER(srv) {
			if (srv->active) {
				close(srv->socket);
			}
		}
		close(sockfd);
		close(newsockfd);
		close(mgmtsock);
		close(clsock);

		exit(1);
	}
}

/**
 * Each q2 server connection gets one of these threads
 */
void *ServerThread(void *arg)
{
	msg_buffer_t msg; // for receiving data only
	uint32_t _ret, serverkey, q2arevision, sock, threadid;
	uint16_t port;
	uint8_t maxclients;
	connection_t _q2con;
	q2_server_t *q2;
	byte cmd;
	ssize_t bytessent = 0;
	size_t readsz = 0;
	//SSL *ssl;
	//SSL_CTX *ctx;
	const SSL_METHOD *method;
	//BIO *io, *ssl_bio;
	byte challenge[CHALLENGE_LEN];
	byte sv_challenge[CHALLENGE_LEN];
	size_t ciphersz;
	byte cipher[200];
	unsigned char cl_challenge[256];
	byte challenge_cipher[256];
	int challenge_cipherlen;
	hello_t hello;
	uint8_t tmp;


	threadid = pthread_self();

	sock = *(uint32_t *) arg;

	memset(&msg, 0, sizeof(msg_buffer_t));
	memset(&challenge, 0, sizeof(challenge));
	memset(&hello, 0, sizeof(hello_t));

	_ret = recv(sock, &msg.data, sizeof(msg.data), 0);

	printf("ret = %d\n", _ret);

	// socket closed on other end
	if (_ret == 0) {
		return NULL;
	}

	printf(" New client\n");

	tmp = MSG_ReadByte(&msg);
	printf("%d\n", tmp);
	// first thing should be hello
	//if (MSG_ReadByte(&msg) != CMD_HELLO) {
	if (tmp != CMD_HELLO) {
		printf("thread[%d] - protocol error, not a real client\n", threadid);
		return NULL;
	}

	ParseHello(&hello, &msg);

	FOR_EACH_SERVER(q2) {
		if (hello.key == q2->key) {
			printf("server found: %d\n", hello.key);
			q2->socket = sock;
			q2->connected = true;
			q2->port = port;
			q2->maxclients = maxclients;

			// encrypt client's challenge to send back and auth server
			//Client_PublicKey_Encypher(q2, &challenge_cypher[0], &hello.challenge[0], &challenge_cypherlen);
			challenge_cipherlen = Sign_Client_Challenge(&challenge_cipher[0], &hello.challenge[0]);

			// generate random data to auth the client
			RAND_bytes(&sv_challenge[0], CHALLENGE_LEN);

			// q2 server won't send any more data until it receives this ACK
			MSG_WriteByte(SCMD_HELLOACK, &q2->msg);
			MSG_WriteShort(challenge_cipherlen, &q2->msg);
			MSG_WriteData(&challenge_cipher[0], challenge_cipherlen, &q2->msg);
			MSG_WriteData(&sv_challenge[0], CHALLENGE_LEN, &q2->msg);
			SendBuffer(q2);

			break;
		}
	}

	// not a valid server
	if (!q2) {
		q2_server_t invalid_server;
		printf("Invalid serverkey, closing connection\n");
		invalid_server.socket = sock;
		MSG_WriteByte(-1, &invalid_server.msg);
		MSG_WriteByte(ERR_UNAUTHORIZED, &invalid_server.msg);
		MSG_WriteString("Server key invalid", &invalid_server.msg);
		SendBuffer(&invalid_server);

		close(sock);
		return NULL;
	}

	printf("Client <%d> version %d connected\n", hello.key, hello.version);

	// read the encrypted challenge
	_ret = recv(sock, &msg.data, sizeof(msg.data), 0);

	ciphersz = MSG_ReadShort(&msg);
	MSG_ReadData(&msg, &cipher, ciphersz);

	printf("thread[%d] - server: %d\n", threadid, serverkey);

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
			case CMD_PLAYERLIST:
				ParsePlayerList(q2, &msg);
				break;
			case CMD_MAP:
				ParseMap(q2, &msg);
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
 * Each q2 server connection gets one of these threads
 */
void *TLSServerThread(void *arg)
{
	msg_buffer_t msg; // for receiving data only
	uint32_t _ret, serverkey, q2arevision, sock, threadid;
	uint16_t port;
	uint8_t maxclients;
	connection_t _q2con;
	q2_server_t *q2;
	byte cmd;
	ssize_t bytessent = 0;
	size_t readsz = 0;
	SSL *ssl;
	SSL_CTX *ctx;
	const SSL_METHOD *method;
	BIO *io, *ssl_bio;
	byte challenge[4];
	size_t ciphersz;
	byte cipher[200];
	unsigned char cl_challenge[256];
	byte challenge_cypher[256];
	uint8_t challenge_cypherlen;


	threadid = pthread_self();

	sock = *(uint32_t *) arg;

	// set the method to TLS (should default to TLS1.2)
	if ((method = TLS_server_method()) < 0) {
		printf("Error setting TLS method, closing connection and stopping thread\n");
		close(sock);
		return NULL;
	}

	// create a new SSL context
	if ((ctx = SSL_CTX_new(method)) < 0) {
		printf("Error creating TLS context, closing connection and stopping thread\n");
		close(sock);
		return NULL;
	}

	// set the SSL certificate to use
	//if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) < 0) {
	if (SSL_CTX_use_certificate_file(ctx, config.certificate, SSL_FILETYPE_PEM) < 0) {
		printf("Error setting up certificate\n");
		SSL_CTX_free(ctx);
		close(sock);
		return NULL;
	}

	// set the private key for the certificate
	//if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) < 0) {
	if (SSL_CTX_use_PrivateKey_file(ctx, config.private_key, SSL_FILETYPE_PEM) < 0) {
		printf("Error setting up the private key\n");
		SSL_CTX_free(ctx);
		close(sock);
		return NULL;
	}

	if ((ssl = SSL_new(ctx)) < 0) {
		printf("Error creating SSL object\n");
		SSL_CTX_free(ctx);
		close(sock);
		return NULL;
	}

	SSL_set_fd(ssl, sock);
	SSL_accept(ssl);

	// setup buffered in/out over TLS
	io = BIO_new(BIO_f_buffer());
	ssl_bio = BIO_new(BIO_f_ssl());
	BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
	BIO_push(io, ssl_bio);

	memset(&msg, 0, sizeof(msg_buffer_t));
	memset(&challenge, 0, sizeof(challenge));

	// only wait a max of 5 seconds during the connection/auth process for each message
	SSL_CTX_set_timeout(ctx, 5);

	readsz = BIO_read(ssl_bio, &msg.data, sizeof(msg.data));

	printf("DEBUG: Read %d bytes: %s\n", strlen(msg.data), msg.data);

	// first thing should be hello
	if (MSG_ReadByte(&msg) != CMD_HELLO) {
		printf("thread[%d] - protocol error, not a real client\n", threadid);
		return NULL;
	}

	serverkey = MSG_ReadLong(&msg);
	q2arevision = MSG_ReadLong(&msg);
	port = MSG_ReadShort(&msg);
	maxclients = MSG_ReadByte(&msg);
	MSG_ReadData(&msg, cl_challenge, CHALLENGE_LEN);

	printf("Client <%d> version %d connected\n", serverkey, q2arevision);

	/**
	 * Generate a challenge (a few random bytes of data) and send to client.
	 * Client will encrypt and send back. We decrypt and if it matches, client
	 * is trusted.
	 */
	RAND_bytes(&challenge[0], sizeof(challenge));
	MSG_WriteByte(SCMD_HELLOACK, &msg);
	MSG_WriteData(&challenge, sizeof(challenge), &msg);
	BIO_write(ssl_bio, &msg.data, msg.length);
	BIO_flush(ssl_bio);

	// read the encrypted challenge
	readsz = BIO_read(ssl_bio, &msg.data, sizeof(msg.data));


	ciphersz = MSG_ReadShort(&msg);
	MSG_ReadData(&msg, &cipher, ciphersz);

	printf("thread[%d] - server: %d\n", threadid, serverkey);

	// from this point we're auth'd, message can come in much slower
	SSL_CTX_set_timeout(ctx, 300);

	FOR_EACH_SERVER(q2) {
		if (serverkey == q2->key) {
			q2->socket = sock;
			q2->connected = true;
			q2->port = port;
			q2->maxclients = maxclients;
			q2->bio = ssl_bio;

			//Client_PublicKey_Encypher(q2, &challenge_cypher[0], &cl_challenge[0]);

			// q2 server won't send any more data until it receives this ACK
			MSG_WriteByte(SCMD_HELLOACK, &q2->msg);
			MSG_WriteShort(256, &q2->msg);
			MSG_WriteData(&challenge_cypher[0], 256, &q2->msg);
			SendBuffer(q2);

			break;
		}
	}

	// not a valid server
	if (!q2) {
		q2_server_t invalid_server;
		printf("Invalid serverkey, closing connection\n");
		invalid_server.socket = sock;
		MSG_WriteByte(-1, &invalid_server.msg);
		MSG_WriteByte(ERR_UNAUTHORIZED, &invalid_server.msg);
		MSG_WriteString("Server key invalid", &invalid_server.msg);
		SendBuffer(&invalid_server);

		BIO_free_all(ssl_bio);
		BIO_free_all(io);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		SSL_CTX_free(ctx);

		close(sock);
		return NULL;
	}

	// main server connection loop
	while (true) {
		memset(&msg, 0, sizeof(msg_buffer_t));
		errno = 0;

		// read data from q2 server
		//msg.length = recv(q2->socket, &msg.data, sizeof(msg.data), 0);
		msg.length = BIO_read(q2->bio, &msg.data, sizeof(msg.data));
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
			case CMD_PLAYERLIST:
				ParsePlayerList(q2, &msg);
				break;
			case CMD_MAP:
				ParseMap(q2, &msg);
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

	if (srv->tls && !srv->bio) {
		return;
	}

	printf("sending (%d): %s\n", srv->msg.length, srv->msg.data);

	if (srv->tls) {
		BIO_write(srv->bio, srv->msg.data, srv->msg.length);
		BIO_flush(srv->bio);
	} else {
		send(srv->socket, srv->msg.data, srv->msg.length, 0);
	}

	memset(&srv->msg, 0, sizeof(msg_buffer_t));

	return;
}

void CloseConnection(q2_server_t *srv)
{
	close(srv->socket);
	srv->connected = false;
}

/**
 * This is a separate thread that will listen for vanilla unencrypted connections
 */
void *Listener(void *arg)
{
	uint32_t sock, cl_sock, clin_size, thread_id;
	struct sockaddr_in servaddr, cliaddr;
	int option = 1;

	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	//OpenSSL_add_all_algorithms();
	//SSL_load_error_strings();

	servaddr.sin_family = AF_UNSPEC; // IPv4 + IPv6
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(config.port);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return NULL;
	}

	// avoid annoying "address already in use errors when closing and opening sockets quickly
	setsockopt(sock, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &option, sizeof(option));

	if (bind(sock, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		return NULL;
	}

	// start listening
	if (listen(sock, 100) < 0) {
		perror("listen");
		return NULL;
	}

	printf("Listening on tcp/%d\n", config.port);

	while (true) {
		cl_sock = accept(sock, (struct sockaddr *) &cliaddr, &clin_size);
		if (cl_sock < 0) {
			perror("accept");
			continue;
		}

		thread_id = find_available_thread_slot();

		// hand connection off to new thread
		pthread_create(&threads[thread_id], NULL, ServerThread, &cl_sock);
	}

	return NULL;
}

/**
 * This is a separate thread that will listen for SSL/TLS connections
 */
void *TLS_Listener(void *arg)
{
	uint32_t sock, cl_sock, clin_size, thread_id;
	struct sockaddr_in servaddr, cliaddr;

	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	servaddr.sin_family = AF_UNSPEC; // IPv4 + IPv6
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(config.tls_port);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return NULL;
	}

	if (bind(sock, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		return NULL;
	}

	// start listening
	if (listen(sock, 100) < 0) {
		perror("listen");
		return NULL;
	}

	printf("Listening on tcp/%d (tls)\n", config.tls_port);

	while (true) {
		cl_sock = accept(sock, (struct sockaddr *) &cliaddr, &clin_size);
		if (cl_sock < 0) {
			perror("accept");
			continue;
		}

		thread_id = find_available_thread_slot();

		// hand connection off to new thread
		pthread_create(&threads[thread_id], NULL, TLSServerThread, &cl_sock);
	}

	return NULL;
}


/**
 * Entry point
 */
int main(int argc, char **argv)
{
	/*
	uint32_t ret, sv_sock, cl_sock;
	struct sockaddr_in servaddr, cliaddr;
	uint32_t clin_size = 0;
	pthread_t threadid, clientthread;
	connection_t conn;
	SSL *ssl;
	SSL_CTX *ssl_ctx;
	const SSL_METHOD *ssl_method;
*/
	threadrunning = false;

	memset(&threads, 0, sizeof(pthread_t) * MAX_THREADS);

	List_Init(&connlist);

	// load the config
	LoadConfig(CONFIGFILE);

	// connect to the database
	db = mysql_init(NULL);
	if (mysql_real_connect(db, config.db_host, config.db_user, config.db_pass, config.db_schema, 0, NULL, 0) == NULL) {
		fprintf(stderr, "%s\n", mysql_error(db));
		mysql_close(db);
		exit(1);
	}

	printf("Connected to database\n");

	LoadServers();

	(void)signal(SIGINT, SignalCatcher);

	//pthread_create(&threads[0], NULL, TLS_Listener, NULL);
	Listener(NULL);

	return EXIT_SUCCESS;
}

