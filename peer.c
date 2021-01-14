#include "server.h"


static void ip_to_bytes(const char *ip, byte *out)
{
    struct sockaddr_in sa;

    inet_pton(AF_INET, ip, &(sa.sin_addr));
    memcpy(out, &(sa.sin_addr), 4);
}

/**
 * generate a list of all active servers to give to a peer
 */
void P_GetServerList(uint32_t index)
{
    msg_buffer_t msg;
    q2_server_t *s;
    byte ipdata[4];
    uint16_t count = 0;

    printf("[info] peer connected\n");

    FOR_EACH_SERVER(s) {
        if (s->connected && s->trusted) {
            count++;
        }
    }

    memset(&msg, 0, sizeof(msg_buffer_t));
    MSG_WriteShort(count, &msg);

    FOR_EACH_SERVER(s) {
        if (s->connected && s->trusted) {
            ip_to_bytes(s->ip, ipdata);
            MSG_WriteData(ipdata, 4, &msg); // q2 is ipv4 only, so always 4 bytes
            MSG_WriteShort(s->port, &msg);
            MSG_WriteString(s->name, &msg);
        }
    }

    send(sockets[index].fd, msg.data, msg.length, 0);
    close(sockets[index].fd);
}
