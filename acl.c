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
	int idx = 0;

	if (sc_get(c, "users/#user", value, sizeof(value)) != 0)
		return;
	count = atoi(value);
	if (!count) {
		printf("No Users\n");
		return;
	}

	/* NULL terminated list */
	users = malloc(sizeof(char *) * (count + 1));
	assert(users);
	memset(users, 0, sizeof(char *) * (count + 1));

	for (id = 0; id < count; id++) {
		snprintf(req, sizeof_safe(req), "users/user[%d]/@name", id+1);
		if (sc_get(c, req, value, sizeof(value)))
			continue;
		users[idx++] = strdup(value);
	}
	count = id;
		
	printf("%d users\n", idx);
	for (id = 0; id < idx; id++) {
		printf("  %s\n", users[id]);
	}

	irc->users = users;
}
