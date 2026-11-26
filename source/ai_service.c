/*
 * AI Service Integration Layer Implementation for BitchX
 */

#include "ai_service.h"
#include "ai_profile.h"
#include "ai_menu.h"
#include "ai_provider.h"
#include "ai_honcho.h"
#include "ai_orm.h"

#ifdef HAVE_CONFIG_H
#include "irc.h"
#include "struct.h"
#include "output.h"
#include "server.h"
#include "window.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef HAVE_CONFIG_H
#define put_it(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define send_to_server(fmt, ...) printf("[SERVER] " fmt "\n", ##__VA_ARGS__)
#define from_server 0
static char *get_server_nickname(int s) { (void)s; return "bitchx_user"; }
static char *get_target_by_refnum(int r) { (void)r; return "#general"; }
#endif

/* Fetch memory context for chat completion */
static void build_memory_context(const AIProfile *prof, const char *channel, const char *nick,
                                 char *out_ctx, size_t max_len) {
    if (!prof || !out_ctx || max_len == 0) return;
    out_ctx[0] = '\0';

    if (prof->memory_type == AI_MEMORY_EXTERNAL_HONCHO) {
        char session_id[128];
        char peer_id[128];
        ai_honcho_resolve_ids(&prof->honcho_config, channel, nick,
                              session_id, sizeof(session_id),
                              peer_id, sizeof(peer_id));

        AIHonchoResponse resp;
        if (ai_honcho_get_peer_context(&prof->honcho_config, peer_id, &resp) == 0 && !resp.is_error) {
            snprintf(out_ctx, max_len, "Peer Context for %s:\n%.2000s\n", peer_id, resp.raw_json);
        }
    } else {
        /* Internal SQLite store */
        AIORMHandle *db = ai_orm_connect(prof->internal_config.backend, prof->internal_config.db_path);
        if (db) {
            AIMemoryRecord memories[5];
            int n_mem = ai_orm_query_memories(db, prof->name, channel, 5, memories, 5);
            size_t off = 0;
            if (n_mem > 0) {
                off += snprintf(out_ctx + off, max_len - off, "Known channel memories:\n");
                for (int i = 0; i < n_mem && off < max_len - 100; i++) {
                    off += snprintf(out_ctx + off, max_len - off, "- [%.50s]: %.150s\n",
                                    memories[i].topic, memories[i].summary);
                }
            }
            ai_orm_disconnect(db);
        }
    }
}

/* Save turn to memory */
static void persist_chat_turn(const AIProfile *prof, const char *channel, const char *user_nick,
                              const char *user_msg, const char *ai_reply) {
    if (!prof) return;

    if (prof->memory_type == AI_MEMORY_EXTERNAL_HONCHO) {
        char session_id[128];
        char peer_id[128];
        ai_honcho_resolve_ids(&prof->honcho_config, channel, user_nick,
                              session_id, sizeof(session_id),
                              peer_id, sizeof(peer_id));
        AIHonchoResponse resp;
        ai_honcho_add_message(&prof->honcho_config, session_id, peer_id, "user", user_msg, &resp);
        ai_honcho_add_message(&prof->honcho_config, session_id, peer_id, "assistant", ai_reply, &resp);
    } else {
        AIORMHandle *db = ai_orm_connect(prof->internal_config.backend, prof->internal_config.db_path);
        if (db) {
            AIMessageRecord user_rec;
            memset(&user_rec, 0, sizeof(user_rec));
            snprintf(user_rec.profile_name, sizeof(user_rec.profile_name), "%s", prof->name);
            snprintf(user_rec.channel, sizeof(user_rec.channel), "%s", channel ? channel : "query");
            snprintf(user_rec.nick, sizeof(user_rec.nick), "%s", user_nick ? user_nick : "user");
            snprintf(user_rec.role, sizeof(user_rec.role), "user");
            snprintf(user_rec.content, sizeof(user_rec.content), "%s", user_msg);
            ai_orm_save_message(db, &user_rec);

            AIMessageRecord ai_rec;
            memset(&ai_rec, 0, sizeof(ai_rec));
            snprintf(ai_rec.profile_name, sizeof(ai_rec.profile_name), "%s", prof->name);
            snprintf(ai_rec.channel, sizeof(ai_rec.channel), "%s", channel ? channel : "query");
            snprintf(ai_rec.nick, sizeof(ai_rec.nick), "%s", prof->chat_model);
            snprintf(ai_rec.role, sizeof(ai_rec.role), "assistant");
            snprintf(ai_rec.content, sizeof(ai_rec.content), "%s", ai_reply);
            ai_orm_save_message(db, &ai_rec);

            ai_orm_disconnect(db);
        }
    }
}

BUILT_IN_COMMAND(aicmd) {
    (void)command; (void)subargs; (void)helparg;
    if (!args || !*args) {
        put_it("Usage: /AI [-say] <prompt>");
        return;
    }

    int say_to_channel = 0;
    char *prompt = args;
    if (!strncasecmp(prompt, "-say ", 5)) {
        say_to_channel = 1;
        prompt += 5;
    }

    AIProfileManager *mgr = ai_get_global_mgr();
    const char *channel = get_target_by_refnum(0);
    const char *my_nick = get_server_nickname(from_server);

    AIProfile *prof = ai_profile_get_for_channel(mgr, channel);
    if (!prof) {
        put_it("\033[1;31m[AI Error]\033[0m No active AI profile found. Use /AIPROFILE CREATE first.");
        return;
    }

    put_it("\033[1;36m[AI (%s)]\033[0m Thinking...", prof->chat_model);

    char mem_ctx[4096] = "";
    build_memory_context(prof, channel, my_nick, mem_ctx, sizeof(mem_ctx));

    AIProviderResponse resp;
    int rc = ai_provider_chat(prof, channel, my_nick, prompt, mem_ctx, &resp);
    if (rc != 0 || resp.is_error) {
        put_it("\033[1;31m[AI Error]\033[0m %s", resp.error_msg[0] ? resp.error_msg : "Inference failed");
        return;
    }

    put_it("\033[1;32m[AI (%s)]:\033[0m %s", prof->chat_model, resp.reply);

    if (say_to_channel && channel && *channel) {
        send_to_server("PRIVMSG %s :%s", channel, resp.reply);
    }

    persist_chat_turn(prof, channel, my_nick, prompt, resp.reply);
}

BUILT_IN_COMMAND(aiprofilecmd) {
    (void)subargs; (void)helparg;
    aiprofile_command(command, args);
}

BUILT_IN_COMMAND(savesessioncmd) {
    (void)subargs; (void)helparg;
    savesession_command(command, args);
}

BUILT_IN_COMMAND(restoresessioncmd) {
    (void)subargs; (void)helparg;
    restoresession_command(command, args);
}

/* #lizard forgives -- IRC trigger dispatcher handles mention, query, and autonomous reflex */
void ai_handle_incoming_message(const char *from, const char *to, const char *msg) {
    if (!from || !to || !msg || !*msg) return;

    char *my_nick = get_server_nickname(from_server);
    if (my_nick && !strcasecmp(from, my_nick)) {
        return; /* Do not react to self */
    }

    AIProfileManager *mgr = ai_get_global_mgr();
    AIProfile *prof = ai_profile_get_for_channel(mgr, to);
    if (!prof) return;

    int is_chan = (*to == '#' || *to == '&' || *to == '+' || *to == '!');
    int triggered = 0;
    const char *prompt_body = msg;

    /* Check mention trigger: @mynick or mynick: */
    if (my_nick && (prof->trigger_mode & AI_TRIGGER_MENTION)) {
        char mention_at[128];
        char mention_colon[128];
        snprintf(mention_at, sizeof(mention_at), "@%s", my_nick);
        snprintf(mention_colon, sizeof(mention_colon), "%s:", my_nick);

        if (!strncasecmp(msg, mention_at, strlen(mention_at))) {
            triggered = 1;
            prompt_body = msg + strlen(mention_at);
        } else if (!strncasecmp(msg, mention_colon, strlen(mention_colon))) {
            triggered = 1;
            prompt_body = msg + strlen(mention_colon);
        }
    }

    /* Check private query trigger */
    if (!is_chan && (prof->trigger_mode & AI_TRIGGER_QUERY)) {
        triggered = 1;
        prompt_body = msg;
    }

    /* Check autonomous System 1 reflex if not already directly mentioned */
    if (!triggered && is_chan && (prof->trigger_mode & AI_TRIGGER_AUTONOMOUS)) {
        char reason[256];
        if (ai_provider_evaluate_system1(prof, to, from, msg, reason, sizeof(reason))) {
            put_it("\033[1;35m[AI System 1 Reflex]\033[0m Chime-in triggered on %s: %s", to, reason);
            triggered = 1;
            prompt_body = msg;
        }
    }

    if (triggered) {
        while (*prompt_body && isspace((unsigned char)*prompt_body)) prompt_body++;
        if (!*prompt_body) prompt_body = "Hello";

        char mem_ctx[4096] = "";
        build_memory_context(prof, to, from, mem_ctx, sizeof(mem_ctx));

        AIProviderResponse resp;
        if (ai_provider_chat(prof, to, from, prompt_body, mem_ctx, &resp) == 0 && !resp.is_error) {
            const char *target = is_chan ? to : from;
            send_to_server("PRIVMSG %s :%s", target, resp.reply);
            put_it("\033[1;32m[AI AutoReply -> %s]:\033[0m %s", target, resp.reply);
            persist_chat_turn(prof, to, from, prompt_body, resp.reply);
        }
    }
}
