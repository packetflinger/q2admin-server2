#include "server.h"

/**
 * Helper for printing out binary keys and ciphertext as hex
 */
void hexDump (char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

/**
 * Pick out a value for a given key in the userinfo string
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
 *
 */
float P_KillDeathRatio(q2_player_t *p)
{
	uint32_t deaths;

	deaths = p->suicide_count + p->death_count;
	if (!deaths) {
		return 0.00;
	}

	return ROUNDF(p->kill_count / deaths, 100);
}

void printdata(byte *data, int size)
{
	int i;

	for (i = 0; i < size; i++){
	    if (i > 0)
	    	printf(":");

	    printf("%02X", data[i]);
	}
	printf("\n");
}


/**
 * Send an error message to a client
 */
void SendError(q2_server_t *server, ra_error_t error, uint8_t id, const char *msg)
{
    MSG_WriteByte(SCMD_ERROR, &server->msg);
    MSG_WriteByte(id, &server->msg);
    MSG_WriteByte(error, &server->msg);
    MSG_WriteString(msg, &server->msg);
    SendBuffer(server);
}


/**
 * Catch things like ctrl+c to close open handles
 */
void SignalCatcher(int sig)
{
    uint8_t i;
    q2_server_t *srv;

    // close all sockets and GTFO
    if (sig == SIGINT) {

        for (i=0; i<socket_count; i++) {
            if (sockets[i].fd) {
                close(sockets[i].fd);
            }
        }

        CloseDatabase();
        exit(EXIT_SUCCESS);
    }
}

/**
 * Just for testing threading
 */
void TestThreading(void *arg)
{
    int i = 0;
    while (i++ < 15) {
        printf("%s %d\n", (char *)arg, i);
        sleep(2);
    }
}

/**
 * Variable assignment, just makes building strings easier
 */
char *va(const char *format, ...)
{
    static char strings[8][MAX_STRING_CHARS];
    static uint16_t index;

    char *string = strings[index++ % 8];

    va_list args;

    va_start(args, format);
    vsnprintf(string, MAX_STRING_CHARS, format, args);
    va_end(args);

    return string;
}
