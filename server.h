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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <mysql/mysql.h>
#include <glib.h>

#include <stdbool.h>
#include <pthread.h>

#include <errno.h>

#define PORT		9988
#define MAXLINE 	1390
#define CONFIGFILE	"q2a.ini"
#define VER_REQ     0

#define MAX_STRING_CHARS	1024
#define MAX_TELE_NAME       15

#define CMD_ONLINE		"sv !remote_online"


typedef unsigned char byte;

#define RFL_FRAGS      1 << 0 	// 1
#define RFL_CHAT       1 << 1	// 2
#define RFL_TELEPORT   1 << 2	// 4
#define RFL_INVITE     1 << 3	// 8
#define RFL_FIND       1 << 4	// 16
#define RFL_WHOIS      1 << 5	// 32
#define RFL_DEBUG      1 << 11	// 2047

/**
 * Each server message
 */
struct message {
	uint8_t type;
	byte data[MAXLINE];
	size_t length;
	struct message *next;
};


typedef struct {
        size_t         length;
        uint32_t       index;
        byte           data[0xffff];
} msg_buffer_t;

msg_buffer_t msg;


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
struct q2_server_s {
	uint32_t id;                   // primary key in database table
	uint32_t key;                  // auth key, sent with every msg
	uint32_t version;              // which revision are we running?
	char ip[INET_ADDRSTRLEN];
	uint16_t port;                 // default 27910
	char password[30];             // rcon password
	char map[20];                  // the current map name
	uint8_t maxclients;            // 256 max in q2 protocol
	uint32_t flags;                //
	char name[50];
	char teleportname[MAX_TELE_NAME];
	long lastcontact;              // when did we last see this server?
	bool enabled;                  // owner wants it used
	bool authorized;               // confirmed legit and can be used
	bool active;                   // server is online now
	int sockfd;                    // dedicated socket for rcon commands
	struct addrinfo *addr;         //
	size_t addrlen;                // remove later
	msg_buffer_t msg;              // remove later
	struct q2_server_s *head;      // first server in the list
	struct q2_server_s *next;      // next server entry
	MYSQL *db;                     // this server's database connection
	char encryption_key[1400];     // key for decrypting msgs
};

typedef struct q2_server_s q2_server_t;

/**
 * Means of death.
 * MUST MATCH same enum in q2admin's g_remote.h
 *
 * This is no where near as granular as the MOD in the game mod,
 * but there doesn't seem to be a way to get that information. We
 * must resort to using what gun is in the attacker's hand at the
 * time of the frag.
 */
typedef enum {
	MOD_ENVIRO,		// world (falling, drowning, crushed, laser, etc)
	MOD_BLASTER,
	MOD_SHOTGUN,
	MOD_SSG,
	MOD_MACHINEGUN,
	MOD_CHAINGUN,
	MOD_GRENADE,
	MOD_GRENADELAUNCHER,
	MOD_HYPERBLASTER,
	MOD_ROCKETLAUNCHER,
	MOD_RAILGUN,
	MOD_BFG,
	MOD_OTHER		// unknown mod-specific custom weapon
} mod_t;


/**
 * Commands sent from q2admin game library.
 * Has to match remote_cmd_t in g_remote.h from q2admin
 */
typedef enum {
	CMD_REGISTER,		// server
	CMD_QUIT,			// server
	CMD_CONNECT,		// player
	CMD_DISCONNECT,		// player
	CMD_PLAYERLIST,
	CMD_PLAYERUPDATE,
	CMD_PRINT,
	CMD_TELEPORT,
	CMD_INVITE,
	CMD_SEEN,
	CMD_WHOIS,
	CMD_PLAYERS,
	CMD_FRAG,
	CMD_MAP,
	CMD_AUTHORIZE,
	CMD_HEARTBEAT
} server_cmd_t;

q2a_config_t config;

q2_server_t *server_list;
GQueue *queue;
bool threadrunning;
MYSQL *db;
int sockfd;


extern bool threadrunning;

void MSG_ReadData(void *out, size_t len);
uint8_t MSG_ReadByte(void);
int8_t MSG_ReadChar(void);
uint16_t MSG_ReadShort(void);
int16_t MSG_ReadWord(void);
int32_t MSG_ReadLong(void);
char *MSG_ReadString(void);

void MSG_WriteByte(uint8_t b, msg_buffer_t *buf);
void MSG_WriteShort(uint16_t s, msg_buffer_t *buf);
void MSG_WriteLong(uint32_t l, msg_buffer_t *buf);
void MSG_WriteString(const char *str, msg_buffer_t *buf);
void MSG_WriteData(const void *data, size_t length, msg_buffer_t *buf);

void SendRCON(q2_server_t *srv, const char *fmt, ...);

void CMD_Teleport_f(q2_server_t *srv);
void CMD_Register_f(q2_server_t *srv);
void CMD_Frag_f(q2_server_t *srv);

q2_server_t *find_server(uint32_t key);
q2_server_t *find_server_by_name(const char *name);

void LOG_Frag_f(q2_server_t *srv);
void LOG_Chat_f(q2_server_t *srv);

uint16_t TP_GetServers(q2_server_t srv, char *buffer);

#endif
