#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <mysql/mysql.h>

#define PORT		9988
#define MAXLINE 	1390

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

int main(int argc, char **argv) {
	int sockfd;
	char buffer[MAXLINE];
	char *hello = "Hello from server";
	struct sockaddr_in servaddr, cliaddr;

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
	servaddr.sin_port = htons(PORT);
	
	// Bind the socket with the server address 
	if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
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

	while (1) {
		n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		buffer[n] = '\0';

		printf("db time: %s\n", getNow(db));
		printf("Client : %s\n", buffer);
		//sendto(sockfd, (const char *) hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
		//printf("Hello message sent.\n");
	}

	return EXIT_SUCCESS;
}

