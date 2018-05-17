#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <simpleconfig.h>
#include <malloc.h>
#include <assert.h>
#include "irc.h"


void
read_nopes(irc_t *irc, config_object_t *c)
{
	char req[64];
	char value[256];
	char **nopes;
	int count = 0;
	int id = 0;

	while (1) {
		snprintf(req, sizeof(req), "nopes/@message[%d]", count+1);
		if (sc_get(c, req, value, sizeof(value)) != 0) {
			break;
		}
		count++;
	}

	/* NULL terminated list */
	nopes = malloc(sizeof(char *) * (count + 1));
	assert(nopes);
	memset(nopes, 0, sizeof(char *) * (count + 1));
	irc->nopes = nopes;

	for (id = 0; id < count; id++) {
		snprintf(req, sizeof(req), "nopes/@message[%d]", id+1);
		if (sc_get(c, req, value, sizeof(value)) != 0) {
			break;
		}
		nopes[id] = strdup(value);
		assert(nopes[id]);
	}

	irc->c_nope = count;
	printf("%d NACKs\n", irc->c_nope);
}


int
nope(irc_t *irc, char *nick)
{
	char msg[256];
	int r;

	if (irc->c_nope <= 0) {
		snprintf(msg, sizeof(msg), "%s: No.", nick);
		goto out;
	}

	r = (rand() % irc->c_nope);
	if (strstr(irc->nopes[r], "%s")) {
		snprintf(msg, sizeof(msg), irc->nopes[r], nick);
	} else {
		snprintf(msg, sizeof(msg), irc->nopes[r]);
	}
out:
	return irc_msg(irc->s, irc->channel, msg);
}
