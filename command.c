#include "command.h"
#include "irc.h"
#include "version.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>

typedef struct {
	char *name;
	char *help;
	int (*func)(irc_t *, char *, char *, char *);
} command_table_t;

command_table_t command_table[];

int external_command(irc_t *irc, char *irc_nick, char *command, char *arg);

int
version(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	if (irc_msg(irc->s, irc->channel, _version_) < 0)
		return -1;
	return 1;
}


int
pong(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	if (irc_msg(irc->s, irc->channel, "pong") < 0)
		return -1;
	return 1;
}


int
reload(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	raise(SIGHUP);
	return 1;
}


int
echo(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	if (irc_msg(irc->s, irc->channel, arg) < 0)
		return -1;
	return 1;
}


int
status(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	char buf[256];
	if (irc->task.pid) {
		snprintf(buf, sizeof(buf), "%s: Running %s (pid %d) for %s",
			 irc_nick, irc->task.task, irc->task.pid,
			 irc->task.user);
	} else {
		snprintf(buf, sizeof(buf), "%s: Not doing anything.",
		         irc_nick);
	}
	if (irc_msg(irc->s, irc->channel, buf) < 0)
		return -1;
	return 1;
}

int
help(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	char buf[512];
	char buf2[128];
	int x;

	if (arg == NULL || strlen(arg) == 0) {
		snprintf(buf, sizeof(buf), "%s:", irc_nick);
		for (x = 0; command_table[x].name != NULL; x++) {
			snprintf(buf2, sizeof(buf2), " %s", command_table[x].name);
			strncat(buf, buf2, sizeof(buf) - (strlen(buf) + 1 + strlen(buf2)));
		}
	
		for (x = 0; strlen(irc->commands[x].name); x++) {
			snprintf(buf2, sizeof(buf2), " %s", irc->commands[x].name);
			strncat(buf, buf2, sizeof(buf) - (strlen(buf) + 1 + strlen(buf2)));
		}
		goto out;
	}

	for (x = 0; command_table[x].name != NULL; x++) {
		if (strcmp(arg, command_table[x].name))
			continue;
		if (command_table[x].help == NULL) {
			snprintf(buf, sizeof(buf), "%s: No help for '%s'", irc_nick, arg);
			goto out;
		}
		snprintf(buf, sizeof(buf), "%s: %s", irc_nick, command_table[x].help);
		goto out;
	}

	for (x = 0; strlen(irc->commands[x].name); x++) {
		if (strcmp(arg, irc->commands[x].name))
			continue;
		if (strlen(irc->commands[x].help) == 0) {
			snprintf(buf, sizeof(buf), "%s: No help for '%s'", irc_nick, arg);
			goto out;
		}
		snprintf(buf, sizeof(buf), "%s: %s", irc_nick, irc->commands[x].help);
		goto out;
	}

	snprintf(buf, sizeof(buf), "%s: Command not found", irc_nick);
out:
	if (irc_msg(irc->s, irc->channel, buf) < 0)
		return -1;
	return 1;
}

int
bonk(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	raise(SIGPIPE);
	return -1;
}


command_table_t command_table[] = {
	{ "status", "Display status", status },
	{ "version", "Display version", version },
	{ "echo", "Print something", echo },
	{ "ping", "Pong!", pong },
	{ "reload", "Reload configuration", reload },
	{ "help", NULL, help },
	{ "bonk", "Reconnect", bonk },
	{ NULL, NULL, NULL }
};


int
process_command(irc_t *irc, char *irc_nick, char *msg)
{
	char **words;
	char *command;
	char *arg;
	int x, ret;

	words = strsplit(msg, " \t");
	command = words[0];
	if (command == NULL)
		return 0;
	arg = words[1];

	for (x = 0; command_table[x].name != NULL; x++) {
		if (strcmp(command, command_table[x].name))
			continue;
		ret = command_table[x].func(irc, irc_nick, command, arg);
		goto out;
	}

	ret = external_command(irc, irc_nick, command, arg);
out:
	free(words);
	return ret;
}

