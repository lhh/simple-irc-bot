#include "socket.h"
#include "irc.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "command.h"


void nope(irc_t *irc, char *irc_nick, char *command, char *arg);


int
irc_connect(irc_t * irc, const char *server, const char *port)
{
	if ((irc->s = get_socket(server, port)) < 0) {
		return -1;
	}
	irc->bufptr = 0;
	return 0;
}

int
irc_login(irc_t * irc, const char *nick)
{
	int ret;

	ret = irc_reg(irc->s, nick, "laas", "SimpleBot");
	if (ret > 0) {
		irc->nick = strdup(nick);
	}
	return ret;
}

int
irc_join_channel(irc_t * irc, const char *channel)
{
	if (strcmp(irc->channel, channel) == 0)
		return 0;
	if (strlen(irc->channel)) {
		irc_leave_channel(irc);
	}
	strncpy(irc->channel, channel, 254);
	irc->channel[254] = '\0';
	return irc_join(irc->s, channel);
}

int
irc_leave_channel(irc_t * irc)
{
	return irc_part(irc->s, irc->channel);
}

int
irc_handle_data(irc_t * irc)
{
	char tempbuffer[512];
	int rc, i;
	if ((rc = sck_recv(irc->s, tempbuffer, sizeof (tempbuffer) - 2)) <= 0) {
		if (rc == 0) {
			/* Select going high then a read of 0 => connection lost */
			raise(SIGPIPE);
		}
		switch(errno) {
			case EINTR:
				return -1;
			case ETIMEDOUT:
				/* send a ping to the server to see
			   	if the connection is still alive */
				if (irc_ping(irc->s, irc->server) < 0) {
					break;
				}
				return 0;
		}
		perror("read");
		return -1;
	}
	tempbuffer[rc] = '\0';
	for (i = 0; i < rc; ++i) {
		switch (tempbuffer[i]) {
		case '\r':
		case '\n':
			{
				irc->servbuf[irc->bufptr] = '\0';
				irc->bufptr = 0;
				if (irc_parse_action(irc) < 0)
					return -1;
				break;
			}
		default:
			{
				irc->servbuf[irc->bufptr] = tempbuffer[i];
				if (irc->bufptr >= (sizeof (irc->servbuf) - 1))
					// Overflow!
					;

				else
					irc->bufptr++;
			}
		}
	}
	return 0;
}

int
irc_parse_action(irc_t * irc)
{
	char *ptr;
	char irc_nick[128];
	char irc_msg[512];
	int privmsg = 0;
	int ret;

	*irc_nick = '\0';
	*irc_msg = '\0';

	if (strncmp(irc->servbuf, "PING :", 6) == 0) {
		return irc_pong(irc->s, &irc->servbuf[6]);
	}

	else if (strncmp(irc->servbuf, "NOTICE AUTH :", 13) == 0) {
		// Don't care
		return 0;
	}

	else if (strncmp(irc->servbuf, "ERROR :", 7) == 0) {
		// Still don't care
		return 0;
	}

	// Parses IRC message that pulls out nick and message. 
	// Checks if we have non-message string
	if (irc->servbuf[0] != ':')
		return 0;

	ptr = NULL;
	if (strchr(irc->servbuf, '!')) {
		ptr = strtok(irc->servbuf, "!");
	}

	irc_nick[0] = 0;
	if (ptr) {
		strncpy(irc_nick, &ptr[1], 127);
		irc_nick[127] = '\0';
	} else {
		ptr = strtok(irc->servbuf, " ");
	}

	while ((ptr = strtok(NULL, " ")) != NULL) {
		if (strcmp(ptr, "PONG") == 0) {
			return 0;
		}
		if (strcmp(ptr, "PRIVMSG") == 0) {
			privmsg = 1;
			break;
		}
	}
	if (privmsg) {
		if ((ptr = strtok(NULL, ":")) != NULL
		    && (ptr = strtok(NULL, "")) != NULL) {
			strncpy(irc_msg, ptr, 511);
			irc_msg[511] = '\0';
		}
	}
	if (privmsg == 1 && strlen(irc_nick) > 0 && strlen(irc_msg) > 0) {
		ret = irc_reply_message(irc, irc_nick, irc_msg);
		if (ret < 0)
			return -1;
		else if (ret > 0)
			irc_log_message(irc, irc_nick, irc_msg);
	}

	return 0;
}

int
irc_set_output(irc_t * irc, const char *file)
{
	irc->file = fopen(file, "w");
	if (irc->file == NULL)
		return -1;
	return 0;
}

int
irc_reply_message(irc_t * irc, char *irc_nick, char *msg)
{
	char buf[256];
	int s;

	s = snprintf(buf, sizeof_safe(buf), "%s: ", irc->nick);
        if (strncmp(msg, buf, s) != 0) {
		s = snprintf(buf, sizeof_safe(buf), "%s, ", irc->nick);
		if (strncmp(msg, buf, s) != 0) {
			return 0;
		}
	}

	return process_command(irc, irc_nick, &msg[s]);
}

int
irc_log_message(irc_t * irc, const char *nick, const char *message)
{
	char timestring[128];
	time_t curtime;
	time(&curtime);
	strftime(timestring, 127, "%F - %H:%M:%S", localtime(&curtime));
	timestring[127] = '\0';
	fprintf(irc->file, "%s - [%s] <%s> %s\n", irc->channel, timestring,
		nick, message);
	fflush(irc->file);
	return 0;
}

void
irc_init(irc_t *irc)
{
	if (!irc)
		return;
	memset(irc, 0, sizeof(*irc));
	irc->s = -1;
}

void
irc_clear_config(irc_t *irc)
{
	int x;

	if (irc->users) {
		for (x = 0; irc->users[x] != NULL; x++) {
			free(irc->users[x]);
		}
		free(irc->users);
		irc->users = NULL;
	}
	if (irc->nopes) {
		for (x = 0; irc->nopes[x] != NULL; x++) {
			free(irc->nopes[x]);
		}
		free(irc->nopes);
		irc->nopes = NULL;
	}
	if (irc->commands) {
		free(irc->commands);
		irc->nopes = NULL;
	}
}

void
irc_close(irc_t * irc)
{
	if (!irc) {
		return;
	}
	if (irc->s >= 0) {
		close(irc->s);
		irc->s = -1;
	}
}

void
irc_shutdown(irc_t * irc)
{
	irc_close(irc);
	irc_clear_config(irc);
	if (irc->file) {
		fclose(irc->file);
		irc->file = (FILE *)NULL;
	}
}

// irc_ping: For answering pong requests...
int
irc_ping(int s, const char *data)
{
	return sck_sendf(s, "PING %s\r\n", data);
}

// irc_pong: For answering pong requests...
int
irc_pong(int s, const char *data)
{
	return sck_sendf(s, "PONG :%s\r\n", data);
}

// irc_reg: For registering upon login
int
irc_reg(int s, const char *nick, const char *username, const char *fullname)
{
	return sck_sendf(s, "NICK %s\r\nUSER %s localhost 0 :%s\r\n", nick,
			 username, fullname);
}

// irc_join: For joining a channel
int
irc_join(int s, const char *data)
{
	return sck_sendf(s, "JOIN %s\r\n", data);
}

// irc_part: For leaving a channel
int
irc_part(int s, const char *data)
{
	return sck_sendf(s, "PART %s\r\n", data);
}

// irc_nick: For changing your nick
int
irc_nick(int s, const char *data)
{
	return sck_sendf(s, "NICK %s\r\n", data);
}

// irc_quit: For quitting IRC
int
irc_quit(int s, const char *data)
{
	return sck_sendf(s, "QUIT :%s\r\n", data);
}

// irc_topic: For setting or removing a topic
int
irc_topic(int s, const char *channel, const char *data)
{
	return sck_sendf(s, "TOPIC %s :%s\r\n", channel, data);
}

// irc_action: For executing an action (.e.g /me is hungry)
int
irc_action(int s, const char *channel, const char *data)
{
	return sck_sendf(s, "PRIVMSG %s :\001ACTION %s\001\r\n", channel, data);
}

// irc_msg: For sending a channel message or a query
int
irc_msg(int s, const char *channel, const char *data)
{
	return sck_sendf(s, "PRIVMSG %s :%s\r\n", channel, data);
}
