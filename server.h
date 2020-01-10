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

#define ROUNDF(f,c) (((float)((int)((f) * (c))) / (c)))

#define PORT		9988
#define MAXLINE 	1390
#define CONFIGFILE	"q2a.ini"
#define VER_REQ     0

#define MAX_STRING_CHARS	1024
#define MAX_TELE_NAME       15
#define MAX_NAME_CHARS		15	// playername
#define MAX_USERINFO_CHARS	512
#define MAX_THREADS         256

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
 * Represents an active player
 */
typedef struct {
	uint8_t client_id;
	char name[MAX_NAME_CHARS];
	char userinfo[MAX_USERINFO_CHARS];
	uint32_t kill_count;
	uint32_t death_count;
	uint32_t suicide_count;
} q2_player_t;


/**
 * This represents a new q2 server connection,
 * before we know which server it belongs with
 */
typedef struct {
	uint32_t thread_id;
	uint32_t socket;
} connection_t;


/**
 * Represents a server record in the database. For speed sake, these records are
 * loaded into these structures. When user updates the website, these are reloaded
 */
struct q2_server_s {
	bool connected;
	uint32_t socket;
	uint32_t id;                   // primary key in database table
	uint32_t key;                  // auth key, sent with every msg
	uint32_t version;              // which revision are we running?
	char ip[INET_ADDRSTRLEN];
	uint16_t port;                 // default 27910
	char password[30];             // rcon password
	char map[20];                  // the current map name
	uint8_t playercount;
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
	q2_player_t players[256];
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
 * Has to match ra_client_cmd_t in g_remote.h from q2admin
 *
 * Major client (q2server) to server (q2admin server)
 * commands.
 */
typedef enum {
	CMD_NULL,
	CMD_HELLO,
	CMD_QUIT,
	CMD_CONNECT,       // player
	CMD_DISCONNECT,    // player
	CMD_PLAYERLIST,
	CMD_PLAYERUPDATE,
	CMD_PRINT,
	CMD_COMMAND,       // teleport, invite, etc
	CMD_PLAYERS,
	CMD_FRAG,          // someone fragged someone else
	CMD_MAP,           // map changed
	CMD_PING           //
} ra_client_cmd_t;

/**
 * Server to client commands
 */
typedef enum {
	SCMD_NULL,
	SCMD_HELLOACK,
	SCMD_PONG,
	SCMD_COMMAND,
	SCMD_SAYCLIENT,
} ra_server_cmd_t;

typedef enum {
	CMD_COMMAND_TELEPORT,
	CMD_COMMAND_INVITE,
	CMD_COMMAND_WHOIS
} remote_cmd_command_t;

typedef enum {
	PRINT_LOW,      // pickups
	PRINT_MEDIUM,   // obits
	PRINT_HIGH,     // critical
	PRINT_CHAT,     // chat (duh)
} print_level_t;

q2a_config_t config;

q2_server_t *server_list;
GQueue *queue;
bool threadrunning;
MYSQL *db;
int sockfd, newsockfd;
pthread_t threads[MAX_THREADS];
uint32_t thread_count;

extern bool threadrunning;

void      MSG_ReadData(msg_buffer_t *msg, void *out, size_t len);
uint8_t   MSG_ReadByte(msg_buffer_t *msg);
int8_t    MSG_ReadChar(msg_buffer_t *msg);
uint16_t  MSG_ReadShort(msg_buffer_t *msg);
int16_t   MSG_ReadWord(msg_buffer_t *msg);
int32_t   MSG_ReadLong(msg_buffer_t *msg);
char      *MSG_ReadString(msg_buffer_t *msg);

void      MSG_WriteByte(uint8_t b, msg_buffer_t *buf);
void      MSG_WriteShort(uint16_t s, msg_buffer_t *buf);
void      MSG_WriteLong(uint32_t l, msg_buffer_t *buf);
void      MSG_WriteString(const char *str, msg_buffer_t *buf);
void      MSG_WriteData(const void *data, size_t length, msg_buffer_t *buf);

void      SendBuffer(q2_server_t *srv);

void      CMD_Teleport_f(q2_server_t *srv);
void      CMD_Register_f(q2_server_t *srv);
void      CMD_Frag_f(q2_server_t *srv);
void      CMD_PlayerConnect_f(q2_server_t *srv);
void      CMD_PlayerDisconnect_f(q2_server_t *srv);

void      CloseConnection(q2_server_t *srv);

q2_server_t *find_server(uint32_t key);
q2_server_t *find_server_by_name(const char *name);

void      *ServerThread(void *arg);

void      LOG_Frag_f(q2_server_t *srv);
void      LOG_Chat_f(q2_server_t *srv);

char      *va(const char *format, ...);
char      *Info_ValueForKey(char *s, char *key);

void      TP_GetServers(q2_server_t *srv, uint8_t player, char *target);

void      ParsePrint(q2_server_t *srv, msg_buffer_t *in);
void      ParseCommand(q2_server_t *srv, msg_buffer_t *in);
void      ParseTeleport(q2_server_t *srv, msg_buffer_t *in);
void      ParseInvite(q2_server_t *srv, msg_buffer_t *in);
void      ParseWhois(q2_server_t *srv, msg_buffer_t *in);
void      ParsePlayerConnect(q2_server_t *srv, msg_buffer_t *in);
void      ParsePlayerUpdate(q2_server_t *srv, msg_buffer_t *in);
void      ParsePlayerDisconnect(q2_server_t *srv, msg_buffer_t *in);
void      ParseMap(q2_server_t *srv, msg_buffer_t *in);
void      ParsePlayerList(q2_server_t *srv, msg_buffer_t *in);

#endif
