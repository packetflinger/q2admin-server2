#include "server.h"


static void ip_to_bytes(const char *ip, byte *out)
{
    struct sockaddr_in sa;
    inet_pton(AF_INET, ip, &(sa.sin_addr));
    memcpy(out, &sa.sin_addr.s_addr, 4);
    printf("%d\n", out[0]);
}

/**
 * generate a list of all active servers to give to a peer
 */
void P_GetServerList(void)
{
    q2_server_t *s;
    byte ipdata[4];

    FOR_EACH_SERVER(s) {
        if (s->connected) {
            ip_to_bytes(s->ip, ipdata);
        }
    }
}
