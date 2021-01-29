#include "command.h"
#include "irc.h"
#include "version.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <malloc.h>

typedef struct {
	char *name;
	char *help;
	int (*func)(irc_t *, char *, char **);
} command_table_t;

command_table_t command_table[];

int external_command(irc_t *irc, char *irc_nick, char **argv);

int
version(irc_t *irc, char *irc_nick, char **argv)
{

	if (irc_msg(irc->s, irc->channel, _version_) < 0)
		return -1;
	return 1;
}


int
pong(irc_t *irc, char *irc_nick, char **argv)
{
	if (irc_msg(irc->s, irc->channel, "pong") < 0)
		return -1;
	return 1;
}


int
reload(irc_t *irc, char *irc_nick, char **argv)
{
	raise(SIGHUP);
	return 1;
}


int
echo(irc_t *irc, char *irc_nick, char **argv)
{
	char buf[256];
	char *command = argv[0];
	int max, x;

	memset(buf, 0, sizeof(buf));

	if (!argv[1]) {
		snprintf(buf, sizeof_safe(buf), "%s: %s what?", command, irc_nick);
	} else {
		x = snprintf(buf, sizeof_safe(buf), "%s: ", irc_nick);
		max = sizeof(buf) - x - 1;
		x = 1;
		while (argv[x] && max > strlen(argv[x])+2) {
			strcat(buf, argv[x]);
			strcat(buf, " ");
			max -= (strlen(argv[x]) + 1);
			x++;
		}
	}

	if (irc_msg(irc->s, irc->channel, buf) < 0)
		return -1;
	return 1;
}


int
status(irc_t *irc, char *irc_nick, char **argv)
{
	char buf[256];
	task_node_t *t;
	int x;

	if (irc->tasks) {
		list_for(&irc->tasks, t, x) {
			snprintf(buf, sizeof_safe(buf),
                         	"%s: PID%d for %s: \"%s\", %ld sec",
				irc_nick, t->task.pid,
				t->task.user,
				t->task.task,
				time(NULL) - t->task.start_time);
			irc_msg(irc->s, irc->channel, buf);
		}
	} else {
		snprintf(buf, sizeof_safe(buf), "%s: Not doing anything.",
		         irc_nick);
		if (irc_msg(irc->s, irc->channel, buf) < 0)
			return -1;
	}
	return 1;
}

int
help(irc_t *irc, char *irc_nick, char **argv)
{
	char *arg = argv[1];
	char buf[512];
	char buf2[128];
	int x;

	if (arg == NULL || strlen(arg) == 0) {
		snprintf(buf, sizeof_safe(buf), "%s:", irc_nick);
		for (x = 0; command_table[x].name != NULL; x++) {
			snprintf(buf2, sizeof_safe(buf2), " %s", command_table[x].name);
			strncat(buf, buf2, sizeof(buf) - (strlen(buf) + 1 + strlen(buf2)));
		}
	
		for (x = 0; strlen(irc->commands[x].name); x++) {
			snprintf(buf2, sizeof_safe(buf2), " %s", irc->commands[x].name);
			strncat(buf, buf2, sizeof(buf) - (strlen(buf) + 1 + strlen(buf2)));
		}
		goto out;
	}

	for (x = 0; command_table[x].name != NULL; x++) {
		if (strcmp(arg, command_table[x].name))
			continue;
		if (command_table[x].help == NULL) {
			snprintf(buf, sizeof_safe(buf), "%s: No help for '%s'", irc_nick, arg);
			goto out;
		}
		snprintf(buf, sizeof_safe(buf), "%s: %s", irc_nick, command_table[x].help);
		goto out;
	}

	for (x = 0; strlen(irc->commands[x].name); x++) {
		if (strcmp(arg, irc->commands[x].name))
			continue;
		if (strlen(irc->commands[x].help) == 0) {
			snprintf(buf, sizeof_safe(buf), "%s: No help for '%s'", irc_nick, arg);
			goto out;
		}
		snprintf(buf, sizeof_safe(buf), "%s: %s", irc_nick, irc->commands[x].help);
		goto out;
	}

	snprintf(buf, sizeof_safe(buf), "%s: Command not found", irc_nick);
out:
	if (irc_msg(irc->s, irc->channel, buf) < 0)
		return -1;
	return 1;
}

int
bonk(irc_t *irc, char *irc_nick, char **argv)
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
	int x, ret;

	words = strsplit(msg, " \t");
	command = words[0];
	if (command == NULL)
		return 0;

	for (x = 0; command_table[x].name != NULL; x++) {
		if (strcmp(command, command_table[x].name))
			continue;
		ret = command_table[x].func(irc, irc_nick, words);
		goto out;
	}

	ret = external_command(irc, irc_nick, words);
out:
	free(words);
	return ret;
}

