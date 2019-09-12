#ifndef SERVER_H
#define SERVER_H

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

#include <stdbool.h>
#include <pthread.h>

#define PORT		9988
#define MAXLINE 	1390
#define CONFIGFILE	"q2a.ini"

typedef unsigned char byte;


/**
 * Each server message
 */
struct message {
	uint8_t type;
	byte data[MAXLINE];
	size_t length;
	struct message *next;
};


/**
 * General configuration options for the main q2admin process
 */
typedef struct {
	uint16_t port;
	char db_host[50];
	char db_user[20];
	char db_pass[20];
	char db_schema[20];
} q2a_config_t;


/**
 * Represents a server record in the database. For speed sake, these records are
 * loaded into these structures. When user updates the website, these are reloaded
 */
typedef struct {
	uint32_t id;			// primary key in database table
	uint32_t key;			// auth key, sent with every msg
	byte ip[4];
	uint16_t port;			// default 27910
	char password[30];		// rcon password
	char map[20];			// the current map name
	uint8_t maxclients;		// 256 max in q2 protocol
	uint32_t flags;			//
	char name[50];
	char teleportname[15];
	long lastcontact;		// when did we last see this server?
	bool enabled;			// owner wants it used
	bool authorized;		// confirmed legit and can be used
	struct q2_server_t	*next;		// next server entry
} q2_server_t;


extern bool threadrunning;

#endif
