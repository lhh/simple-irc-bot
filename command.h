#ifndef _COMMAND_H
#define _COMMAND_H

#include "irc.h"

int process_command(irc_t *irc, char *irc_nick, char *msg);

#endif
