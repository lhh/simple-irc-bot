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
	char output[4000];
	char msg[4096];
	char *c, *p;
	int status, ret;
	int capture = 0, x;
	task_node_t *t = NULL;

	if (!irc->tasks) {
		return 0;
	}

	ret = waitpid(-1, &status, WNOHANG);
	if (ret < 0) {
		return 0;
	}

	list_for(&irc->tasks, t, x) {
		if (t->task.pid == ret) {
			ret = 0;
			break;
		}
	}

	if (ret != 0) {
		/* couldn't find one? */
		printf("XXX Woke up but no processes?\n");
		return 0;
	}

	list_remove(&irc->tasks, t);

	if (t->task.fd >= 0) {
		memset(output, 0, sizeof(output));
		ret = read(t->task.fd, output, sizeof(output)-1);
		if (ret < 0) {
			perror("read");
		}
		if (ret > 0) {
			capture = 1;
		}
		close(t->task.fd);
		t->task.fd = -1;
	}

	if (capture) {
		p = output;
		while ((c = strchr(p, '\n'))) {
			*c = 0;
			if (strlen(p)) {
				snprintf(msg, sizeof(msg), "%s: %s", t->task.user,
					 p);
 				irc_msg(irc->s, irc->channel, msg);
			}
			p = c+1;
		}
		if (strlen(p)) {
			snprintf(msg, sizeof(msg), "%s: %s", t->task.user,
				 p);
 			irc_msg(irc->s, irc->channel, msg);
		}
	} else {
		snprintf(msg, sizeof(msg), "%s: %s complete, return code %d",
			 t->task.user, t->task.task, WEXITSTATUS(status));
 		irc_msg(irc->s, irc->channel, msg);
	}

	free(t);
	return 0;
}


void
run_process(irc_t *irc, char *irc_nick, command_t *cmd, char **argv)
{
	char *arg = argv[1];
	char cmdline[512];
	regex_t rx;
	int pid;
	int fd;
	int capture = cmd->capture;
	int p[2];
	task_node_t *task = NULL;

	//printf("Capture: %d\n", capture);

	

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

		pid = regexec(&rx, arg, 0, NULL, 0);
		regfree(&rx);

		if (pid) {
			snprintf(cmdline, sizeof(cmdline), "%s: Value to \'%s\' was invalid",
				 irc_nick, cmd->name);
			irc_msg(irc->s, irc->channel, cmdline);
			goto out;
		}
		snprintf(cmdline, sizeof(cmdline)-1, cmd->action, arg);
	} else {
		snprintf(cmdline, sizeof(cmdline)-1, cmd->action);
	}

	//printf("Running \'%s\' for %s\n", cmdline, irc_nick);

	if (capture) {
		/* WARNING: pipe buffer filling up will deadlock */
		/* Need to create circular buffer and thread to
		   consume pipe output so child task does not
		   block */
		if (pipe(p) < 0) {
			capture = 0;
		}
	}


	pid = fork();
	if (pid < 0) {
		if (capture) {
			close(p[0]);
			close(p[1]);
		}
		goto out;
	}

	if (pid > 0) {
		task = malloc(sizeof(*task));
		memset(task, 0, sizeof(*task));

		/* record who did what */
		snprintf(task->task.user, sizeof(task->task.user), irc_nick);
		if (arg) {
			snprintf(task->task.task, sizeof(task->task.task), "%s %s",
				 cmd->name, arg);
		} else {
			snprintf(task->task.task, sizeof(task->task.task), "%s",
				 cmd->name);
		}
		task->task.start_time = time(NULL);
		task->task.pid = pid;
		task->task.fd = -1;
		if (!capture) {
			snprintf(cmdline, sizeof(cmdline), "%s: %s started",
				 irc_nick, cmd->name);
			irc_msg(irc->s, irc->channel, cmdline);
		} else {
			task->task.fd = p[0];
			close(p[1]);
		}
		goto out;
	}

	close(irc->s);

	if (!capture) {
		fd = open("/dev/null", O_RDWR);
		for (pid = 0; pid <= 2; pid++) {
			if (fd != pid) {
				close(pid);
				dup2(fd, pid);
			}
		}
	} else {
		close(p[0]);
		close(0);
		for (pid = 1; pid <= 2; pid++) {
			if (p[1] != pid) {
				close(pid);
				dup2(p[1], pid);
			}
		}
	}
        execl("/bin/sh", "sh", "-c", cmdline, (char *) 0);
	/* notreached */
	return;
out:
	if (task)
		list_insert(&irc->tasks, task);
	/* parent */
	return;
}


int
external_command(irc_t *irc, char *irc_nick, char **argv)
{
	char *command = argv[0];
	int s, user_valid = 0, n;

	/* TODO - move this to another function */
	for (n = 0; irc->users[n] != NULL; n++) {
		/* Meh, easy to spoof - use only on IRC
		   servers with authentication */
		if (!strcmp(irc_nick, irc->users[n]))
			user_valid = 1;
	}

	for (s = 0; irc->commands[s].name[0]; s++) {
		if (strcmp(command, irc->commands[s].name))
			continue;
		if (!user_valid && !irc->commands[s].anon) {
			printf("Invalid user: %s\n", irc_nick);
			goto out;
		}

		run_process(irc, irc_nick, &irc->commands[s], argv);
		return 1;
	}
out:
	nope(irc, irc_nick);
	return 0;
}


void
read_commands(irc_t *irc, config_object_t *c)
{
	char req[64];
	char value[64];
	command_t *commands;
	int count = 0;
	int id = 0;
	int idx = 0;

	if (sc_get(c, "commands/#command", value, sizeof(value)) != 0)
		return;
	count = atoi(value);

	if (!count) {
		printf("No Commands\n");
		return;
	}

	/* NULL terminated list */
	commands = malloc(sizeof(command_t) * (count + 1));
	assert(commands);
	memset(commands, 0, sizeof(command_t) * (count + 1));

	for (id = 0; id < count; id++) {
		snprintf(req, sizeof(req), "commands/command[%d]/@name", id+1);
		if (sc_get(c, req, value, sizeof(value)) != 0) {
			continue;
		}
		++idx;
		strncpy(commands[idx-1].name, value, sizeof(commands[idx-1].name));

		snprintf(req, sizeof(req), "commands/command[%d]/@action", id+1);
		if (sc_get(c, req, commands[idx-1].action, sizeof(commands[id].action)) != 0)
			/* no action */
			continue;
		/* don't care if there's no help */
		snprintf(req, sizeof(req), "commands/command[%d]/@help", id+1);
		sc_get(c, req, commands[idx-1].help, sizeof(commands[id].help));
		snprintf(req, sizeof(req), "commands/command[%d]/@capture", id+1);
		commands[id].capture = 0;
		if (sc_get(c, req, value, sizeof(value)) == 0) {
			if (atoi(value) > 0 || strcasecmp(value, "yes") || strcasecmp(value, "true")) {
				//printf("Capturing for %s (%s)\n", commands[id].name, value);
				commands[idx-1].capture = 1;
			}
		}
		snprintf(req, sizeof(req), "commands/command[%d]/@anon", id+1);
		commands[id].anon = 0;
		if (sc_get(c, req, value, sizeof(value)) == 0) {
			if (atoi(value) > 0 || strcasecmp(value, "yes") || strcasecmp(value, "true")) {
				//printf("Capturing for %s (%s)\n", commands[id].name, value);
				commands[idx-1].anon = 1;
			}
		}
		snprintf(req, sizeof(req), "commands/command[%d]/@regex", id+1);
		sc_get(c, req, commands[idx-1].regex, sizeof(commands[id].regex));
	}
		
	printf("%d actions\n", idx);
	for (id = 0; id < idx; id++) {
		printf("  %s: %s\n", commands[id].name, commands[id].action);
	}

	irc->commands = commands;
}
