/*
 * Universal AI Provider Client for BitchX (OpenAI-compatible HTTP API)
 * Supports OpenAI, Anthropic (via LiteLLM/OpenCode), Ollama, OpenRouter, MiniMax, etc.
 * Includes System 1 reactive evaluation for autonomous conversation chime-in.
 */

#ifndef _AI_PROVIDER_H_
#define _AI_PROVIDER_H_

#include <stddef.h>
#include "ai_profile.h"

#define AI_MAX_CHAT_REPLY_LEN 8192

typedef struct {
    char reply[AI_MAX_CHAT_REPLY_LEN];
    long status_code;
    int is_error;
    char error_msg[256];
} AIProviderResponse;

/* Initialize provider networking */
int  ai_provider_init(void);
void ai_provider_cleanup(void);

/* Build the full completions URL from profile endpoint */
void ai_provider_resolve_url(const char *endpoint, char *out_url, size_t max_len);

/* System 1 reactive evaluation: decides whether the AI assistant should chime in.
 * Returns 1 if System 1 decides to reply (chime in), 0 if NO or error.
 */
int  ai_provider_evaluate_system1(const AIProfile *profile, const char *channel,
                                  const char *nick, const char *message,
                                  char *out_reason, size_t reason_buflen);

/* System 2 full chat completion with memory context injection */
int  ai_provider_chat(const AIProfile *profile, const char *channel,
                      const char *nick, const char *prompt,
                      const char *memory_context, AIProviderResponse *resp);

#endif /* _AI_PROVIDER_H_ */
