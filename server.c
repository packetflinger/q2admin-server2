
#include "server.h"

LIST_DECL(q2srvlist);

struct pollfd *sockets;
uint32_t socket_size = 0;
uint32_t socket_count = 0;


/**
 * Loads config from file. Uses glib2's ini parsing stuff
 */
void LoadConfig(char *filename)
{
	// problems reading file, just use default port
	if (access(filename, F_OK) == -1) {
	    printf("Problems loading config file '%s', aborting.\n", filename);
	    exit(EXIT_FAILURE);
	}

	printf("Loading config from '%s'\n", filename);

	g_autoptr(GError) error = NULL;
	g_autoptr(GKeyFile) key_file = g_key_file_new ();

	if (!g_key_file_load_from_file(key_file, filename, G_KEY_FILE_NONE, &error)) {
		if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			g_warning ("Error loading key file: %s", error->message);
		}
	    return;
	}

	gchar *val;

	val = g_key_file_get_string(key_file, "database", "file", &error);
	if (val) {
	    strncpy(config.db_file, val, sizeof(config.db_file));
	}

	gint val2 = g_key_file_get_integer(key_file, "server", "port", &error);
	config.port = (uint16_t) val2;

	val = g_key_file_get_string(key_file, "crypto", "private_key", &error);
	strncpy(config.private_key, val, sizeof(config.private_key));

	val = g_key_file_get_string(key_file, "crypto", "public_key", &error);
	strncpy(config.public_key, val, sizeof(config.public_key));

	g_free(val);
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
 * Fetch enabled servers from the database and load into a list
 */

bool LoadServers(void)
{
    q2_server_t *temp, *server;
    uint32_t ret;
    sqlite3_stmt *res;

    List_Init(&q2srvlist);
    printf("Loading servers from database...\n");

    if (!db) {
        printf("[warn] database not open\n");
        return false;
    }

    ret = sqlite3_prepare_v2(db, "SELECT * FROM server WHERE enabled = 1", -1, &res, 0);
    if (ret != SQLITE_OK) {
        return false;
    }

    while ((ret = sqlite3_step(res)) == SQLITE_ROW) {
        temp = malloc(sizeof(q2_server_t));
        memset(temp, 0x0, sizeof(q2_server_t));

        temp->id = sqlite3_column_int(res, 0);
        temp->key = sqlite3_column_int(res, 2);
        temp->port = sqlite3_column_int(res, 6);
        temp->enabled = sqlite3_column_int(res, 7);
        temp->flags = sqlite3_column_int(res, 3);
        strncpy(temp->name, sqlite3_column_text(res, 4), sizeof(temp->name));
        strncpy(temp->ip, sqlite3_column_text(res, 5), sizeof(temp->ip));

        List_Append(&q2srvlist, &temp->entry);
    }

    sqlite3_finalize(res);

    FOR_EACH_SERVER(server) {
        printf("- %s %s:%d\n", server->name, server->ip, server->port);
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
		if (strcmp(name, s->name) == 0) {
			return s;
		}
	}

	return NULL;
}

/**
 * Send a reply to a q2 server PING request
 */
void Pong(q2_server_t *srv)
{
	MSG_WriteByte(SCMD_PONG, &srv->msg);
	SendBuffer(srv);
}


/**
 * When a client connects, load their public key into memory for
 * decrypting their auth challenge and encrypting their symmetric key/IV
 */
void LoadClientPublicKey(q2_server_t *q2)
{
    FILE *fp;
    char keyfilename[200];

    memset(keyfilename, 0, sizeof(keyfilename));
    snprintf(keyfilename, sizeof(keyfilename), "keys/%d.pem", q2->key);
    fp = fopen(keyfilename, "rb");

    q2->publickey = RSA_new();

    if (q2->publickey) {
        q2->publickey = PEM_read_RSAPublicKey(fp, &q2->publickey, NULL, NULL);
    }

    fclose(fp);
}


/**
 * A client sent us a challenge. Encrypt it and send it back along with
 * our challenge to them (and any encryption keys required)
 *
 */
bool ServerAuthResponse(q2_server_t *q2, byte *challenge)
{
    size_t len;
    byte cl_challenge[CHALLENGE_LEN];
    byte cipher[RSA_LEN];

    // generate random data for a challenge and store for later comparison
    RAND_bytes(cl_challenge, CHALLENGE_LEN);
    strncpy(q2->connection.cl_challenge, cl_challenge, CHALLENGE_LEN);

    // encrypt client's challenge to send back and auth server
    len = Sign_Client_Challenge(cipher, challenge);
    if (len == 0) {
        RSA_free(q2->publickey);
        return false;
    }

    // client won't send any more data until it receives this ACK
    MSG_WriteByte(SCMD_HELLOACK, &q2->msg);
    MSG_WriteShort(len, &q2->msg);
    MSG_WriteData(cipher, len, &q2->msg);

    // client wants the connection encrypted
    if (q2->connection.encrypted) {
        RAND_bytes(q2->connection.aeskey, AESKEY_LEN);  // session key
        RAND_bytes(q2->connection.iv, AESBLOCK_LEN);    // initialization vector
        Encrypt_AESKey(
                q2->publickey,
                q2->connection.aeskey,
                q2->connection.iv,
                q2->connection.aeskey_cipher
        );
        MSG_WriteData(&q2->connection.aeskey_cipher, RSA_LEN, &q2->msg);
    }

    MSG_WriteData(cl_challenge, CHALLENGE_LEN, &q2->msg);
    SendBuffer(q2);

    return true;
}

/**
 * Decrypt the nonce the client returned to us. If it matches the client is trusted
 */
bool VerifyClientChallenge(q2_server_t *q2, msg_buffer_t *msg)
{
    size_t len;
    size_t count;
    byte cipher[RSA_LEN];
    byte plaintext[CHALLENGE_LEN];

    len = MSG_ReadShort(msg);
    MSG_ReadData(msg, &cipher, len);

    count = RSA_public_decrypt(
            len,
            cipher,
            plaintext,
            q2->publickey,
            RSA_PKCS1_PADDING
    );

    if (count != CHALLENGE_LEN) {
        return false;
    }

    if (memcmp(q2->connection.cl_challenge, plaintext, CHALLENGE_LEN) == 0) {
        q2->connection.e_ctx = EVP_CIPHER_CTX_new();
        q2->connection.d_ctx = EVP_CIPHER_CTX_new();
        q2->trusted = true;
    } else {
        printf("[error] %s connected but is NOT trusted, disconnecting\n", q2->name);
        return false;
    }

    return true;
}


/**
 * Send the contents of the message buffer to the q2 server
 */
void SendBuffer(q2_server_t *srv)
{
    byte buffer[0xffff];
    size_t len;

	if (!srv->connected) {
		return;
	}

	if (!srv->msg.length) {
		return;
	}

	// encrypt if we should
	if (srv->connection.encrypted && srv->trusted) {
	    len = SymmetricEncrypt(srv, buffer, srv->msg.data, srv->msg.length);
	    memset(&srv->msg, 0, sizeof(msg_buffer_t));
	    memcpy(srv->msg.data, buffer, len);
	    srv->msg.length = len;
	}

	send(sockets[srv->index].fd, srv->msg.data, srv->msg.length, 0);

	memset(&srv->msg, 0, sizeof(msg_buffer_t));

	return;
}


/**
 * Add a socket to the poll array
 */
void add_server_socket(uint32_t socket)
{
    sockets[socket_count].fd = socket;
    sockets[socket_count].events = POLLIN;
    socket_count++;
}


/**
 * Remove the given socket from the poll array.
 * Rebuild the array to make sure it's contiguous
 */
void remove_server_socket()
{
    q2_server_t *q2;
    uint32_t i = 1; // 0 is the listener

    FOR_EACH_SERVER(q2) {
        if (!q2->connected) {
            continue;
        }

        sockets[i].fd = q2->socket;
        sockets[i].events = POLLIN;
        q2->index = i;
        i++;
    }

    socket_count = i;
}


/**
 * Close the connection to the client and free any resources it consumed
 */
void CloseConnection(q2_server_t *srv)
{
    if (srv->publickey) {
        RSA_free(srv->publickey);
    }

    if (srv->connection.d_ctx) {
        EVP_CIPHER_CTX_free(srv->connection.d_ctx);
    }

    if (srv->connection.e_ctx) {
        EVP_CIPHER_CTX_free(srv->connection.e_ctx);
    }

    close(srv->socket);
    srv->connected = false;
    printf("%s disconnected\n", srv->name);
    remove_server_socket();
}


/**
 * Create a listener socket and return it
 */
int get_listener_socket(void)
{
    int listener;
    int yes = 1;
    int ret;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static q2_server_t *get_server(uint32_t index)
{
    q2_server_t *q2;

    FOR_EACH_SERVER(q2) {
        if (q2->index == index) {
            return q2;
        }
    }

    return NULL;
}

/**
 * Identify the new incoming server connection
 */
static q2_server_t *new_server(msg_buffer_t *msg, uint32_t index)
{
    hello_t h;
    q2_server_t *q2;

    if (MSG_ReadByte(msg) != CMD_HELLO) {
        return NULL;
    }

    ParseHello(&h, msg);

    FOR_EACH_SERVER(q2) {
        if (h.key == q2->key) {
            q2->index = index;
            q2->socket = sockets[index].fd;
            q2->connected = true;
            q2->port = h.port;
            q2->maxclients = h.max_clients;
            q2->connection.encrypted = h.encrypted;

            LoadClientPublicKey(q2);

            printf("Found server and loaded key: %d\n", &q2->publickey);

            if (!ServerAuthResponse(q2, h.challenge)) {
                MSG_WriteByte(SCMD_ERROR, &q2->msg);
                MSG_WriteByte(-1, &q2->msg);
                MSG_WriteByte(ERR_ENCRYPTION, &q2->msg);
                MSG_WriteString("Problem encrypting sv_challenge", &q2->msg);
                SendBuffer(q2);

                //ERR_CloseConnection(q2);
                return NULL;
            }

            return q2;
            break;
        }
    }

    return NULL;
}


/**
 * Main program loop. Listen for incoming connections, process data from existing
 * connections.
 */
void RunServer(void)
{
    uint32_t listener;
    uint32_t newsocket;

    q2_server_t *q2;
    msg_buffer_t msg;

    uint32_t poll_count = 0;

    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char remote_addr[INET6_ADDRSTRLEN];

    FOR_EACH_SERVER(q2) {
        socket_size++;
    }

    // add 5 or so extra for temporary peer connections
    sockets = malloc(socket_size * sizeof(struct pollfd) + 5);

    listener = get_listener_socket();
    if (listener == -1) {
        fprintf(stderr, "[error] problems getting listening socket\n");
        exit(EXIT_FAILURE);
    }

    sockets[0].fd = listener;
    sockets[0].events = POLLIN;
    socket_count = 1;

    while (true) {
        poll_count = poll(sockets, socket_count, -1);   // block
        if (poll_count == -1) {
            perror("[error] poll");
            exit(EXIT_FAILURE);
        }

        for (int i=0; i<socket_count; i++) {
            if (sockets[i].revents & POLLIN) {
                if (sockets[i].fd == listener) {
                    addrlen = sizeof(remoteaddr);
                    newsocket = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                    if (newsocket == -1) {
                        perror("[error] accept");
                    } else {
                        add_server_socket(newsocket);

                        printf("New connection from %s\n",
                                inet_ntop(
                                        remoteaddr.ss_family,
                                        get_in_addr((struct sockaddr*) &remoteaddr),
                                        remote_addr,
                                        INET6_ADDRSTRLEN
                                )
                        );
                    }
                } else {    // just a client
                    q2 = get_server(i);

                    memset(&msg, 0, sizeof(msg_buffer_t));
                    msg.length = recv(sockets[i].fd, msg.data, sizeof(msg.data), 0);


                    if (msg.length <= 0) {
                        if (msg.length == 0) {
                            printf("[warn] %s disconnected abnormally\n", q2->name);
                        } else {
                            perror("[error[ recv");
                        }

                        q2->connected = false;
                        q2->socket = 0;
                        close(sockets[i].fd);
                        remove_server_socket();

                    } else {
                        if (!q2) {
                            q2 = new_server(&msg, i);

                            // not a real client, hang up and move on
                            if (!q2) {
                                printf("[warn] Invalid client, closing connection\n");
                                close(sockets[i].fd);
                                remove_server_socket();
                                continue;
                            }
                        }

                        ParseMessage(q2, &msg);
                    }
                }
            }
        }
    }
}




/**
 * Entry point
 */
int main(int argc, char **argv)
{
	signal(SIGINT, SignalCatcher);

	LoadConfig(CONFIGFILE);
	OpenDatabase();
	LoadServers();
	RunServer();

	return EXIT_SUCCESS;
}
