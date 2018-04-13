#include "socket.h"
#include "irc.h"
#include <simpleconfig.h>
#include <signal.h>

void read_acls(irc_t *, config_object_t *);

int _exiting = 0;

void
sigint_handler(int sig)
{
	_exiting = 1;
}

int
main(int argc, char **argv)
{
	irc_t irc;
	config_object_t *sc = sc_init();
	char server[64];
	char port[8];
	char nick[32];
	char channel[32];

	irc_init(&irc);

	if (argc < 2) {
		fprintf(stderr, "No configuration specified\n");
		goto exit_err;
	}

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

	signal(SIGINT, sigint_handler);

	if (irc_connect(&irc, server, port) < 0) {
		fprintf(stderr, "Connection failed.\n");
		goto exit_err;
	}

	irc_set_output(&irc, "/dev/stdout");

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
	}

	if (_exiting)
		printf("Exiting on SIGINT\n");

	irc_close(&irc);
	return 0;

      exit_err:
	irc_close(&irc);
	exit(1);
}
