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
