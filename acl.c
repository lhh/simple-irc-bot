#include "socket.h"
#include "irc.h"
#include <simpleconfig.h>
#include <irc.h>
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

void
read_acls(irc_t *irc, config_object_t *c)
{
	char req[64];
	char value[64];
	char **users;
	int count = 0;
	int id = 0;

	while (1) {
		snprintf(req, sizeof(req), "users/user[%d]/@name", count+1);
		if (sc_get(c, req, value, sizeof(value)) != 0) {
			break;
		}
		count++;
	}

	if (!count) {
		printf("No Users\n");
		return;
	}

	/* NULL terminated list */
	users = malloc(sizeof(char *) * (count + 1));
	assert(users);
	memset(users, 0, sizeof(char ) * (count + 1));

	for (id = 0; id < count; id++) {
		snprintf(req, sizeof(req), "users/user[%d]/@name", id+1);
		sc_get(c, req, value, sizeof(value));
		users[id] = strdup(value);
	}
		
	printf("%d users\n", count);
	for (id = 0; id < count; id++) {
		printf("  %s\n", users[id]);
	}

	irc->users = users;
}
