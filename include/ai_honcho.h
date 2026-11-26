/*
 * Honcho HTTP Client for BitchX AI Subsystem
 * Connects directly to Honcho memory stores / dialectic endpoints using libcurl.
 * Supports auto-mapping of IRC channels to Honcho sessions and nicks to Honcho peers.
 */

#ifndef _AI_HONCHO_H_
#define _AI_HONCHO_H_

#include <stddef.h>
#include "ai_profile.h"

#define HONCHO_MAX_RESPONSE_LEN 65536

typedef struct {
    char raw_json[HONCHO_MAX_RESPONSE_LEN];
    long status_code;
    int is_error;
    char error_msg[256];
} AIHonchoResponse;

/* Initialize and cleanup curl subsystem */
int  ai_honcho_global_init(void);
void ai_honcho_global_cleanup(void);

/* Helper to resolve session and peer IDs based on profile config and IRC context */
void ai_honcho_resolve_ids(const AIHonchoConfig *cfg, const char *channel, const char *nick,
                           char *out_session_id, size_t session_buflen,
                           char *out_peer_id, size_t peer_buflen);

/* Context retrieval */
int  ai_honcho_get_peer_context(const AIHonchoConfig *cfg, const char *peer_id, AIHonchoResponse *resp);
int  ai_honcho_get_session_context(const AIHonchoConfig *cfg, const char *session_id, AIHonchoResponse *resp);

/* Record conclusion / fact to Honcho memory */
int  ai_honcho_create_conclusion(const AIHonchoConfig *cfg, const char *session_id, const char *peer_id,
                                 const char *conclusion_text, AIHonchoResponse *resp);

/* Send user/assistant message to Honcho session */
int  ai_honcho_add_message(const AIHonchoConfig *cfg, const char *session_id, const char *peer_id,
                           const char *role, const char *content, AIHonchoResponse *resp);

/* Health & connectivity check */
int  ai_honcho_ping(const AIHonchoConfig *cfg, AIHonchoResponse *resp);

#endif /* _AI_HONCHO_H_ */
