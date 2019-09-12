
#include "server.h"

q2a_config_t config;
q2_server_t *server;
GQueue *queue;
bool threadrunning;

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
bool LoadServers(MYSQL *m, q2_server_t *srv)
{
	static MYSQL_RES *res;
	static MYSQL_ROW r;

	q2_server_t *temp;

	printf("  * Loading servers from database *\n");

	if (mysql_query(m, "SELECT * FROM server WHERE enabled = 1")) {
		printf("%s\n", mysql_error(m));
		return false;
	}

	res = mysql_store_result(m);

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

		printf("   * %s (%s)\n", temp->name, temp->teleportname);

		// head
		if (srv == NULL) {
			srv = temp;
		} else {
			srv->next = (struct q2_server_t *) temp;
			srv = (q2_server_t *) srv->next;
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
 * Entry point
 */
int main(int argc, char **argv)
{
	int sockfd;
	char buffer[MAXLINE];
	char *hello = "Hello from server";
	struct sockaddr_in servaddr, cliaddr;
	pthread_t threadid;
	threadrunning = false;

	// load the config
	LoadConfig(CONFIGFILE);

	// connect to the database
	MYSQL *db = mysql_init(NULL);
	if (mysql_real_connect(db, config.db_host, config.db_user, config.db_pass, config.db_schema, 0, NULL, 0) == NULL) {
		fprintf(stderr, "%s\n", mysql_error(db));
		mysql_close(db);
		exit(1);
	}

	queue = g_queue_new();

	server = 0;
	LoadServers(db, server);

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
		n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		buffer[n] = '\0';


		printf("db time: %s\n", getNow(db));
		printf("Client : %s\n", buffer);

		// add incoming message to queue

		if (!threadrunning) {
			pthread_join(threadid, NULL);
		}

		//sendto(sockfd, (const char *) hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
		//printf("Hello message sent.\n");
	}

	return EXIT_SUCCESS;
}

