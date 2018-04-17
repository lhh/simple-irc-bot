#include "socket.h"
#include "irc.h"
#include <simpleconfig.h>
#include <signal.h>

void read_acls(irc_t *, config_object_t *);
void read_commands(irc_t *irc, config_object_t *c);
int process_done(irc_t *irc);
void irc_clear_config(irc_t *irc);
void read_nopes(irc_t *irc, config_object_t *c);

static int _exiting;
static int _child;
static int _reload;

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

	irc_init(&irc);

	if (argc < 2) {
		fprintf(stderr, "No configuration specified\n");
		goto exit_err;
	}

	signal(SIGINT, sig_handler);
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

	if (sc_get(sc, "server/@channel", channel, sizeof(channel)) != 0) {
		goto exit_err;
	}

	if (sc_get(sc, "server/@port", port, sizeof(port)) != 0) {
		snprintf(port, sizeof(port)-1, "6667");
	}

	if (sc_get(sc, "server/@nick", nick, sizeof(nick)) != 0) {
		snprintf(nick, sizeof(nick)-1, "laas");
	}

	read_acls(&irc, sc);
	read_commands(&irc, sc);
	read_nopes(&irc, sc);

	/* FIXME: Switch channels */
	if (irc.s < 0) {
		irc_set_output(&irc, "/dev/stdout");

		if (irc_connect(&irc, server, port) < 0) {
			fprintf(stderr, "Connection failed.\n");
			goto exit_err;
		}
	}

	if (irc_login(&irc, nick) < 0) {
		fprintf(stderr, "Couldn't log in.\n");
		goto exit_err;
	}

	if (irc_join_channel(&irc, channel) < 0) {
		fprintf(stderr, "Couldn't join channel.\n");
		goto exit_err;
	}

	while ((irc_handle_data(&irc) >= 0) && (!_exiting)) {
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
	}

	if (_exiting)
		printf("Exiting on SIGINT\n");

	irc_close(&irc);
	return 0;

      exit_err:
	irc_close(&irc);
	exit(1);
}
