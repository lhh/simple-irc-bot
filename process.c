#include <stdlib.h>
#include <signal.h>
#include <stdlib.h>
#include <irc.h>
#include <simpleconfig.h>
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>



void
check_commands(irc_t *irc, char *nick, char *msg)
{
	return;
}

void
read_commands(irc_t *irc, config_object_t *c)
{
	char req[64];
	char value[64];
	command_t *commands;
	int count = 0;
	int id = 0;

	while (1) {
		snprintf(req, sizeof(req), "commands/command[%d]/@name", count+1);
		if (sc_get(c, req, value, sizeof(value)) != 0) {
			break;
		}
		snprintf(req, sizeof(req), "commands/command[%d]/@action", count+1);
		if (sc_get(c, req, value, sizeof(value)) != 0)
			/* no action */
			continue;
		count++;
	}

	if (!count) {
		printf("No Commands\n");
		return;
	}

	/* NULL terminated list */
	commands = malloc(sizeof(command_t) * (count + 1));
	assert(commands);
	memset(commands, 0, sizeof(command_t) * (count + 1));

	for (id = 0; id < count;) {
		snprintf(req, sizeof(req), "commands/command[%d]/@name", id+1);
		if (sc_get(c, req, commands[id].name, sizeof(commands[id].name)) != 0) {
			break;
		}
		snprintf(req, sizeof(req), "commands/command[%d]/@action", id+1);
		if (sc_get(c, req, commands[id].action, sizeof(commands[id].action)) != 0)
			/* no action */
			continue;
		id++;
	}
		
	printf("%d actions\n", count);
	for (id = 0; id < count; id++) {
		printf("  %s: %s\n", commands[id].name, commands[id].action);
	}

	irc->commands = commands;
}
