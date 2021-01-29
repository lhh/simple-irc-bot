#include "socket.h"
#include "irc.h"
#include <simpleconfig.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

void read_acls(irc_t *, config_object_t *);
void read_commands(irc_t *irc, config_object_t *c);
int process_done(irc_t *irc);
void irc_clear_config(irc_t *irc);
void read_nopes(irc_t *irc, config_object_t *c);

static int _exiting;
static int _child;
static int _reload;
static int _reconn;

void
sig_handler(int sig)
{
	switch(sig) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		_exiting = 1;
		break;
	case SIGCHLD:
		_child = 1;
		break;
	case SIGWINCH:
	case SIGHUP:
		_reload = 1;
		break;
	case SIGPIPE:
		_reconn = 1;
		break;
	}
}

void
sigchld_handler(int sig)
{
	_child = 1;
}

int
main(int argc, char **argv)
{
	irc_t irc;
	config_object_t *sc = NULL;
	char server[64];
	char port[8];
	char nick[32];
	char channel[32];

restart:
	irc_init(&irc);

	if (argc < 2) {
		fprintf(stderr, "No configuration specified\n");
		goto exit_err;
	}

	signal(SIGINT, sig_handler);
	signal(SIGPIPE, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGHUP, sig_handler);

reload:
	if (sc != NULL) {
		sc_release(sc);
	}
	sc = sc_init();
	irc_clear_config(&irc);

	if (sc_parse(sc, argv[1])) {
		fprintf(stderr, "Failed to parse: %s\n", argv[1]);
		goto exit_err;
	}

	if (sc_get(sc, "server/@hostname", server, sizeof(server)) != 0) {
		fprintf(stderr, "No host specified in %s\n", argv[1]);
		goto exit_err;
	}
	strncpy(irc.server, server, sizeof(irc.server));

	if (sc_get(sc, "server/@channel", channel, sizeof(channel)) != 0) {
		goto exit_err;
	}

	if (sc_get(sc, "server/@port", port, sizeof(port)) != 0) {
		snprintf(port, sizeof_safe(port), "6667");
	}

	if (sc_get(sc, "server/@nick", nick, sizeof(nick)) != 0) {
		snprintf(nick, sizeof_safe(nick), "laas");
	}

	read_acls(&irc, sc);
	read_commands(&irc, sc);
	read_nopes(&irc, sc);

	irc_set_output(&irc, "/dev/stdout");

	if (irc.s < 0) {
		while (irc_connect(&irc, server, port) < 0) {
			if (_exiting) {
				goto exit_err;
			}
			sleep(30);
		}
	}

	if (irc_login(&irc, nick) < 0) {
		fprintf(stderr, "Couldn't log in.\n");
		goto exit_err;
	}
	fprintf(stderr, "Set nick to %s\n", nick);

	if (irc_join_channel(&irc, channel) < 0) {
		fprintf(stderr, "Couldn't join channel.\n");
		goto exit_err;
	}
	fprintf(stderr, "Joined %s\n", channel);

	while (!_exiting) {
		irc_handle_data(&irc);

		if (_exiting) {
			break;
		}
		if (_child) {
			_child = 0;
			process_done(&irc);
		}
		if (_reload) {
			_reload = 0;
			goto reload;
		}
		if (_reconn) {
			fprintf(stderr, "Reconnecting!\n");
			irc_shutdown(&irc);
			sleep(3);
			_reconn = 0;
			goto restart;
		}
	}


	if (_exiting)
		printf("Exiting on SIGINT\n");

	irc_shutdown(&irc);
	return 0;

      exit_err:
	irc_shutdown(&irc);
	exit(1);
}
