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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/pem.h>

#include <glib.h>
#include <sqlite3.h>

#include <stdbool.h>

#include <errno.h>
#include <signal.h>
#include <poll.h>

#include "list.h"
#include "threadpool.h"

#if __GNUC__ >= 4
#define q_offsetof(t, m)    __builtin_offsetof(t, m)
#else
#define q_offsetof(t, m)    ((size_t)&((t *)0)->m)
#endif

#define clamp(a,b,c)    ((a)<(b)?(a)=(b):(a)>(c)?(a)=(c):(a))
#define random()        ((rand () & 0x7fff) / ((float)0x7fff))
#define ROUNDF(f,c)     (((float)((int)((f) * (c))) / (c)))

#define PORT        "9988"
#define MAXLINE     1390
#define CONFIGFILE  "q2a.ini"
#define VER_REQ     0

#define RSA_BITS        2048   // encryption key length
#define CHALLENGE_LEN   16     // bytes
#define AESKEY_LEN      16     // bytes
#define AESBLOCK_LEN    16
#define RSA_LEN         256    // 2048 bits

#define MAX_STRING_CHARS    1024
#define MAX_TELE_NAME       15
#define MAX_NAME_CHARS      15    // playername
#define MAX_USERINFO_CHARS  512
#define MAX_THREADS         256

#define CMD_ONLINE  "sv !remote_online"


typedef unsigned char byte;

#define RFL_FRAGS      1 << 0   // 1
#define RFL_CHAT       1 << 1   // 2
#define RFL_TELEPORT   1 << 2   // 4
#define RFL_INVITE     1 << 3   // 8
#define RFL_FIND       1 << 4   // 16
#define RFL_WHOIS      1 << 5   // 32
#define RFL_DEBUG      1 << 11  // 2047

#define RFL(f)      ((remote.flags & RFL_##f) != 0)

#define FOR_EACH_SERVER(s) \
    LIST_FOR_EACH(q2_server_t, s, &q2srvlist, entry)

/**
 * Each server message
 */
struct message {
    uint8_t type;
    byte    data[MAXLINE];
    size_t  length;
    struct message *next;
};


typedef struct {
    size_t      length;
    uint32_t    index;
    byte        data[0xffff];
} msg_buffer_t;

msg_buffer_t msg;


/**
 * General configuration options for the main q2admin process
 */
typedef struct {
    uint16_t port;
    char db_file[50];       // sqlite db file
    char private_key[50];   // our key pair
    char public_key[50];    // all clients need this too
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
    uint32_t invite_count;      // how many times this player used invite
    uint32_t invite_frame;      // throttle invite cmd to this frame
} q2_player_t;


/**
 * This represents a new q2 server connection,
 * before we know which server it belongs with
 */
typedef struct {
    uint32_t            thread_id;
    uint32_t            socket;
    SSL                 *ssl;
    SSL_CTX             *ssl_context;
    const SSL_METHOD    *ssl_method;
    struct sockaddr_in  addr;
    uint32_t            addr_len;
    byte                aeskey[AESKEY_LEN];      // plaintext session key
    byte                aeskey_cipher[RSA_LEN];  // encrypted session key + IV
    byte                iv[AESBLOCK_LEN];
    EVP_CIPHER_CTX      *e_ctx;     // encrypting context
    EVP_CIPHER_CTX      *d_ctx;     // decrypting context
    bool                encrypted;
    byte                cl_challenge[CHALLENGE_LEN];
    list_t              entry;
} connection_t;


/**
 * Hello is the first message sent by a client. It contains all
 * the necessary info to register with the server.
 */
typedef struct {
    uint32_t    key;                      // the unique database index
    uint32_t    version;                  // the q2admin game version
    uint16_t    port;                     // the port q2 is running on the client
    uint8_t     max_clients;              // max players on that server
    uint8_t     encrypted;                // is this connection encrypted?
    byte        challenge[CHALLENGE_LEN]; // random data to auth the server
} hello_t;


/**
 * Represents a server record in the database. For speed sake, these records are
 * loaded into these structures. When user updates the website, these are reloaded
 */
struct q2_server_s {
    uint32_t        index;          // the "i" in poll's socket array
    bool            connected;
    uint32_t        socket;
    connection_t    connection;
    uint32_t        id;             // primary key in database table
    uint32_t        key;            // auth key, sent with every msg
    uint32_t        version;        // which revision are we running?
    char            ip[INET_ADDRSTRLEN];
    uint16_t        port;           // default 27910
    char            password[30];   // rcon password
    char            map[20];        // the current map name
    uint8_t         playercount;
    uint8_t         maxclients;     // 256 max in q2 protocol
    uint32_t        flags;
    char            name[50];
    long            lastcontact;    // when did we last see this server?
    bool            enabled;        // owner wants it used
    msg_buffer_t    msg;            // sending
    msg_buffer_t    msg_in;         // receiving
    q2_player_t     players[256];
    bool            trusted;        // auth'd, identity confirmed
    RSA             *publickey;
    list_t          entry;
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
    MOD_ENVIRO,        // world (falling, drowning, crushed, laser, etc)
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
    MOD_OTHER        // unknown mod-specific custom weapon
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
    CMD_PING,           //
    CMD_AUTH
} ra_client_cmd_t;


/**
 * Server to client commands
 */
typedef enum {
    SCMD_NULL,
    SCMD_HELLOACK,
    SCMD_ERROR,
    SCMD_PONG,
    SCMD_COMMAND,
    SCMD_SAYCLIENT,
    SCMD_SAYALL,
    SCMD_AUTH,
    SCMD_TRUSTED,
} ra_server_cmd_t;


/**
 * Q2admin specific commands
 */
typedef enum {
    CMD_COMMAND_TELEPORT,
    CMD_COMMAND_INVITE,
    CMD_COMMAND_WHOIS
} remote_cmd_command_t;


/**
 * Print levels
 */
typedef enum {
    PRINT_LOW,      // pickups
    PRINT_MEDIUM,   // obits
    PRINT_HIGH,     // critical
    PRINT_CHAT,     // chat (duh)
} print_level_t;


/**
 * Possible errors to send to the q2 servers
 * 1xx errors are soft, can be recovered from
 * 2xx errors are hard, causes a disconnect
 */
typedef enum {
    ERR_INVITEQUOTA = 100,
    ERR_TELEPORTQUOTA,
    ERR_UNAUTHORIZED = 200, // server key mismatch
    ERR_OLDVERSION,         // more recent version required
    ERR_ENCRYPTION,         // some encryption error
} ra_error_t;


q2a_config_t config;
sqlite3 *db;
extern list_t q2srvlist;
extern struct pollfd *sockets;
extern uint32_t socket_size;
extern uint32_t socket_count;
extern threadpool pool;

void        MSG_ReadData(msg_buffer_t *msg, void *out, size_t len);
uint8_t     MSG_ReadByte(msg_buffer_t *msg);
int8_t      MSG_ReadChar(msg_buffer_t *msg);
uint16_t    MSG_ReadShort(msg_buffer_t *msg);
int16_t     MSG_ReadWord(msg_buffer_t *msg);
int32_t     MSG_ReadLong(msg_buffer_t *msg);
char        *MSG_ReadString(msg_buffer_t *msg);

void        MSG_WriteByte(uint8_t b, msg_buffer_t *buf);
void        MSG_WriteShort(uint16_t s, msg_buffer_t *buf);
void        MSG_WriteLong(uint32_t l, msg_buffer_t *buf);
void        MSG_WriteString(const char *str, msg_buffer_t *buf);
void        MSG_WriteData(const void *data, size_t length, msg_buffer_t *buf);

void        SendBuffer(q2_server_t *srv);

void        CMD_Teleport_f(q2_server_t *srv);
void        CMD_Register_f(q2_server_t *srv);
void        CMD_Frag_f(q2_server_t *srv);
void        CMD_PlayerConnect_f(q2_server_t *srv);
void        CMD_PlayerDisconnect_f(q2_server_t *srv);

void        CloseConnection(q2_server_t *srv);

q2_server_t *find_server(uint32_t key);
q2_server_t *find_server_by_name(const char *name);

void        *ServerThread(void *arg);

void        LOG_Frag_f(q2_server_t *srv);
void        LOG_Chat_f(q2_server_t *srv);

void        TP_GetServers(q2_server_t *srv, uint8_t player, char *target);

void        Pong(q2_server_t *srv);

void        ParseMessage(q2_server_t *q2, msg_buffer_t *msg);
void        ParsePrint(q2_server_t *srv, msg_buffer_t *in);
void        ParseCommand(q2_server_t *srv, msg_buffer_t *in);
void        ParseTeleport(q2_server_t *srv, msg_buffer_t *in);
void        ParseInvite(q2_server_t *srv, msg_buffer_t *in);
void        ParseWhois(q2_server_t *srv, msg_buffer_t *in);
void        ParsePlayerConnect(q2_server_t *srv, msg_buffer_t *in);
void        ParsePlayerUpdate(q2_server_t *srv, msg_buffer_t *in);
void        ParsePlayerDisconnect(q2_server_t *srv, msg_buffer_t *in);
void        ParseMap(q2_server_t *srv, msg_buffer_t *in);
void        ParsePlayerList(q2_server_t *srv, msg_buffer_t *in);
void        ParseHello(hello_t *h, msg_buffer_t *in);
void        ParseAuth(q2_server_t *q2, msg_buffer_t *in);

void        *ClientThread(void *arg);
void        CL_HandleInput(gchar **in);

// crypto.c
void        Client_PublicKey_Encypher(q2_server_t *q2, byte *to, byte *from, int *len);
size_t      Client_Challenge_Decrypt(q2_server_t *q2, byte *to, byte *from);
uint32_t    Server_PrivateKey_Encypher(byte *to, byte *from);
uint32_t    Server_PrivateKey_Decypher(byte *to, byte *from);

size_t      Sign_Client_Challenge(byte *to, byte *from);
size_t      Encrypt_AESKey(RSA *publickey, byte *key, byte *iv, byte *cipher);
void        hexDump (char *desc, void *addr, int len);
size_t      SymmetricDecrypt(q2_server_t *q2, byte *dest, byte *src, size_t src_len);
size_t      SymmetricEncrypt(q2_server_t *q2, byte *dest, byte *src, size_t src_len);
bool        VerifyClientChallenge(q2_server_t *q2, msg_buffer_t *msg);

// database.c
void        OpenDatabase(void);
void        CloseDatabase(void);

// util.c
void        hexDump (char *desc, void *addr, int len);
char        *Info_ValueForKey(char *s, char *key);
void        SendError(q2_server_t *server, ra_error_t error, uint8_t id, const char *msg);
void        SignalCatcher(int sig);
void        TestThreading(void *arg);
char        *va(const char *format, ...);


#endif
