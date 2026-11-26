/*
 * AI Service Integration Layer for BitchX
 * Bridges BitchX IRC commands and incoming PRIVMSG events to AI profiles,
 * Honcho memory, SQLite ORM, and OpenAI-compatible providers.
 */

#ifndef _AI_SERVICE_H_
#define _AI_SERVICE_H_

#include "irc_std.h"

/* BitchX Command Handlers */
BUILT_IN_COMMAND(aicmd);
BUILT_IN_COMMAND(aiprofilecmd);
BUILT_IN_COMMAND(savesessioncmd);
BUILT_IN_COMMAND(restoresessioncmd);

/* Incoming PRIVMSG hook for mentions, queries, and autonomous System 1 reflex */
void ai_handle_incoming_message(const char *from, const char *to, const char *msg);

#endif /* _AI_SERVICE_H_ */
