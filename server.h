#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <pthread.h>

#define PORT		9988
#define MAXLINE 	1390
#define CONFIGFILE	"q2a.ini"


typedef struct {
	uint16_t port;
} q2a_config_t;

extern bool threadrunning;

#endif
