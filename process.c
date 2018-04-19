#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <irc.h>
#include <simpleconfig.h>
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <regex.h>

int nope(irc_t *irc, char *nick);

int
process_done(irc_t *irc)
{
	char msg[256];
	int status, ret;

	if (!irc->task.pid) {
		return 0;
	}

	ret = waitpid(irc->task.pid, &status, WNOHANG);
	if (ret != irc->task.pid) {
		return 0;
	}

	snprintf(msg, sizeof(msg), "%s: %s complete, return code %d\n",
		 irc->task.user, irc->task.task, WEXITSTATUS(status));

	memset(&irc->task, 0, sizeof(irc->task));
	return irc_msg(irc->s, irc->channel, msg);
}


void
run_process(irc_t *irc, char *irc_nick, command_t *cmd, char *arg)
{
	char cmdline[512];
	regex_t rx;
	int pid;
	int fd;

	if (irc->task.pid) {
		snprintf(cmdline, sizeof(cmdline), "%s: I am busy doing %s for %s\n",
			 irc_nick, irc->task.task, irc->task.user);
		irc_msg(irc->s, irc->channel, cmdline);
		return;
	}

	if (strstr(cmd->action, "%s")) {
		if (arg == NULL) {
			snprintf(cmdline, sizeof(cmdline), "%s: \'%s\' requires an argument",
				irc_nick, cmd->name);
			irc_msg(irc->s, irc->channel, cmdline);
			return;
		}

		/* printf("command: %s arg: \'%s\'\n", cmd->name, arg); */
		if (strlen(cmd->regex)) {
			if (regcomp(&rx, cmd->regex, REG_EXTENDED) != 0) {
				snprintf(cmdline, sizeof(cmdline), "%s: Config regex error\n",
					irc_nick);
				irc_msg(irc->s, irc->channel, cmdline);
				return;
			}
		} else {
			/* default */
			regcomp(&rx, "^[a-zA-Z0-9_,\\.\\-]+$", REG_EXTENDED);
		}

		if (regexec(&rx, arg, 0, NULL, 0)) {
			snprintf(cmdline, sizeof(cmdline), "%s: Value to \'%s\' was invalid",
				 irc_nick, cmd->name);
			irc_msg(irc->s, irc->channel, cmdline);
			goto out;
		}
		snprintf(cmdline, sizeof(cmdline)-1, cmd->action, arg);
	} else {
		snprintf(cmdline, sizeof(cmdline)-1, cmd->action);
	}

	printf("Running \'%s\' for %s\n", cmdline, irc_nick);

	pid = fork();
	if (pid < 0) {
		goto out;
	}

	if (pid > 0) {
		/* record who did what */
		snprintf(irc->task.user, sizeof(irc->task.user), irc_nick);
		snprintf(irc->task.task, sizeof(irc->task.task), cmd->name);
		irc->task.pid = pid;
		snprintf(cmdline, sizeof(cmdline), "%s: %s started",
			 irc_nick, cmd->name);
		irc_msg(irc->s, irc->channel, cmdline);
		goto out;
	}

	close(irc->s);
	fd = open("/dev/null", O_RDWR);
	for (pid = 0; pid <= 2; pid++) {
		if (fd != pid) {
			close(pid);
			dup2(fd, pid);
		}
	}

        execl("/bin/sh", "sh", "-c", cmdline, (char *) 0);
	/* notreached */
	return;
out:
	/* parent */
	regfree(&rx);
}

void
process_command(irc_t *irc, char *irc_nick, char *command, char *arg)
{
	int s, user_valid, n;

	for (s = 0; irc->commands[s].name[0]; s++) {
		if (strcmp(command, irc->commands[s].name)) {
			continue;
		}
		user_valid = 0;
		for (n = 0; irc->users[n] != NULL; n++) {
			/* Meh, easy to spoof - use only on IRC
			   servers with authentication */
			if (!strcmp(irc_nick, irc->users[n])) {
				user_valid = 1;
			}
		}
		if (user_valid) {
			run_process(irc, irc_nick, &irc->commands[s], arg);
		} else {
			printf("Invalid user: %s\n", irc_nick);
			nope(irc, irc_nick);
		}
	}
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
		snprintf(req, sizeof(req), "commands/command[%d]/@regex", id+1);
		sc_get(c, req, commands[id].regex, sizeof(commands[id].regex));
		id++;
	}
		
	printf("%d actions\n", count);
	for (id = 0; id < count; id++) {
		printf("  %s: %s\n", commands[id].name, commands[id].action);
	}

	irc->commands = commands;
}
