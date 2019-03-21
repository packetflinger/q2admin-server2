#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <mysql/mysql.h>
#include <glib.h>

#include "server.h"

q2a_config_t config;
GQueue *queue;
bool threadrunning;

void LoadConfig(char *filename) {
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

	//g_autofree gchar *val = g_key_file_get_string (key_file, "Group Name", "SomeKey", &error);
	gint val = g_key_file_get_integer(key_file, "server", "port", &error);
	config.port = (uint16_t) val;
}

char *getNow(MYSQL *m) {
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

void *ProcessQueue(void *arg) {
	threadrunning = true;
	while (!g_queue_is_empty(queue)) {

	}

	threadrunning = false;
	return NULL;
}

int main(int argc, char **argv) {
	int sockfd;
	char buffer[MAXLINE];
	char *hello = "Hello from server";
	struct sockaddr_in servaddr, cliaddr;
	pthread_t threadid;
	threadrunning = false;

	// load the config
	LoadConfig(CONFIGFILE);

	queue = g_queue_new();

	printf("Listening for Quake 2 traffic on port %d\n", config.port);

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

	MYSQL *db = mysql_init(NULL);
	if (mysql_real_connect(db, "localhost", "root", "", "q2admin", 0, NULL, 0) == NULL) {
	    fprintf(stderr, "%s\n", mysql_error(db));
	    mysql_close(db);
	    exit(1);
	}

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

