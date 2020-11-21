#include "server.h"

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
