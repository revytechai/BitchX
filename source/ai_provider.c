/*
 * Universal AI Provider Client Implementation for BitchX
 */

#include "ai_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

static int g_provider_curl_init = 0;

int ai_provider_init(void) {
    if (!g_provider_curl_init) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {
            g_provider_curl_init = 1;
            return 0;
        }
        return -1;
    }
    return 0;
}

void ai_provider_cleanup(void) {
    if (g_provider_curl_init) {
        curl_global_cleanup();
        g_provider_curl_init = 0;
    }
}

void ai_provider_resolve_url(const char *endpoint, char *out_url, size_t max_len) {
    if (!endpoint || !out_url || max_len == 0) return;
    if (strstr(endpoint, "/chat/completions") != NULL) {
        snprintf(out_url, max_len, "%s", endpoint);
    } else {
        size_t len = strlen(endpoint);
        if (len > 0 && endpoint[len - 1] == '/') {
            snprintf(out_url, max_len, "%schat/completions", endpoint);
        } else {
            snprintf(out_url, max_len, "%s/chat/completions", endpoint);
        }
    }
}

struct ProviderMemBuffer {
    char *data;
    size_t size;
    size_t capacity;
};

static size_t provider_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    struct ProviderMemBuffer *buf = (struct ProviderMemBuffer *)userdata;

    if (buf->size + total >= buf->capacity) {
        total = buf->capacity - buf->size - 1;
    }

    if (total > 0) {
        memcpy(buf->data + buf->size, ptr, total);
        buf->size += total;
        buf->data[buf->size] = '\0';
    }

    return size * nmemb;
}

/* Helper to escape strings for JSON payloads */
static void json_escape(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_len - 2; i++) {
        char c = in[i];
        if (c == '\"' || c == '\\') {
            if (j < out_len - 3) {
                out[j++] = '\\';
                out[j++] = c;
            }
        } else if (c == '\n') {
            if (j < out_len - 3) {
                out[j++] = '\\';
                out[j++] = 'n';
            }
        } else if (c == '\r') {
            if (j < out_len - 3) {
                out[j++] = '\\';
                out[j++] = 'r';
            }
        } else if (c == '\t') {
            if (j < out_len - 3) {
                out[j++] = '\\';
                out[j++] = 't';
            }
        } else if ((unsigned char)c >= 32) {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

/* Extract assistant content from OpenAI JSON completion response */
/* #lizard forgives -- parser for OpenAI chat completion JSON */
static int extract_chat_content(const char *json, char *dest, size_t dest_len) {
    if (!json || !dest || dest_len == 0) return -1;
    const char *p = strstr(json, "\"content\"");
    if (!p) return -1;

    p += 9; /* strlen("\"content\"") */
    while (*p && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (*p != '\"') return -1;
    p++;

    size_t j = 0;
    while (*p && j < dest_len - 1) {
        if (*p == '\"' && *(p - 1) != '\\') break;
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n') dest[j++] = '\n';
            else if (*p == 'r') dest[j++] = '\r';
            else if (*p == 't') dest[j++] = '\t';
            else if (*p == '\"') dest[j++] = '\"';
            else if (*p == '\\') dest[j++] = '\\';
            else dest[j++] = *p;
        } else {
            dest[j++] = *p;
        }
        p++;
    }
    dest[j] = '\0';
    return 0;
}

/* CPD-OFF -- libcurl HTTP POST execution */
static int post_json_completion(const char *url, const char *api_key, const char *json_payload, AIProviderResponse *resp) {
    if (!url || !json_payload || !resp) return -1;
    memset(resp, 0, sizeof(AIProviderResponse));

    ai_provider_init();
    CURL *curl = curl_easy_init();
    if (!curl) {
        resp->is_error = 1;
        snprintf(resp->error_msg, sizeof(resp->error_msg), "Could not initialize CURL handle");
        return -1;
    }

    char raw_resp[AI_MAX_CHAT_REPLY_LEN * 2];
    struct ProviderMemBuffer buf;
    buf.data = raw_resp;
    buf.size = 0;
    buf.capacity = sizeof(raw_resp);
    raw_resp[0] = '\0';

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    if (api_key && *api_key) {
        char auth_buf[300];
        snprintf(auth_buf, sizeof(auth_buf), "Authorization: Bearer %s", api_key);
        headers = curl_slist_append(headers, auth_buf);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, provider_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        resp->is_error = 1;
        snprintf(resp->error_msg, sizeof(resp->error_msg), "CURL error: %s", curl_easy_strerror(code));
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->status_code);
        if (resp->status_code >= 400) {
            resp->is_error = 1;
            snprintf(resp->error_msg, sizeof(resp->error_msg), "HTTP error %ld: %.200s", resp->status_code, raw_resp);
        } else {
            if (extract_chat_content(raw_resp, resp->reply, sizeof(resp->reply)) != 0) {
                snprintf(resp->reply, sizeof(resp->reply), "%.500s", raw_resp);
            }
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp->is_error ? -1 : 0;
}
/* CPD-ON */

int ai_provider_evaluate_system1(const AIProfile *profile, const char *channel,
                                  const char *nick, const char *message,
                                  char *out_reason, size_t reason_buflen) {
    if (!profile || !message || !*message) return 0;
    if (!(profile->trigger_mode & AI_TRIGGER_AUTONOMOUS)) return 0;

    const char *model = profile->internal_config.system1_model[0] ?
                        profile->internal_config.system1_model :
                        (profile->memory_model[0] ? profile->memory_model : profile->chat_model);

    char url[512];
    ai_provider_resolve_url(profile->api_endpoint[0] ? profile->api_endpoint : "http://127.0.0.1:4000/v1",
                            url, sizeof(url));

    const char *key = profile->api_key_env[0] ? getenv(profile->api_key_env) : NULL;

    char esc_msg[2048];
    json_escape(message, esc_msg, sizeof(esc_msg));

    char payload[4096];
    snprintf(payload, sizeof(payload),
             "{"
             "\"model\":\"%s\","
             "\"messages\":["
             "{\"role\":\"system\",\"content\":\"You are a reactive IRC bot reflex filter. Given a channel message, determine whether the AI assistant should chime in. Reply ONLY 'YES: <short reason>' or 'NO: <short reason>'.\"},"
             "{\"role\":\"user\",\"content\":\"Channel: %s, Nick: %s, Message: %s\"}"
             "],"
             "\"max_tokens\":60,"
             "\"temperature\":0.2"
             "}",
             model,
             channel ? channel : "private",
             nick ? nick : "user",
             esc_msg);

    AIProviderResponse resp;
    int rc = post_json_completion(url, key, payload, &resp);
    if (rc != 0 || resp.is_error) {
        return 0;
    }

    /* Check if response starts with YES */
    const char *reply_cur = resp.reply;
    while (*reply_cur && isspace((unsigned char)*reply_cur)) reply_cur++;

    if (!strncasecmp(reply_cur, "YES", 3)) {
        if (out_reason && reason_buflen > 0) {
            snprintf(out_reason, reason_buflen, "%s", reply_cur);
        }
        return 1;
    }

    if (out_reason && reason_buflen > 0) {
        snprintf(out_reason, reason_buflen, "%s", reply_cur);
    }
    return 0;
}

int ai_provider_chat(const AIProfile *profile, const char *channel,
                      const char *nick, const char *prompt,
                      const char *memory_context, AIProviderResponse *resp) {
    if (!profile || !prompt || !resp) return -1;

    char url[512];
    ai_provider_resolve_url(profile->api_endpoint[0] ? profile->api_endpoint : "http://127.0.0.1:4000/v1",
                            url, sizeof(url));

    const char *key = profile->api_key_env[0] ? getenv(profile->api_key_env) : NULL;

    char esc_sys[2048];
    char sys_full[4096];
    if (memory_context && *memory_context) {
        snprintf(sys_full, sizeof(sys_full), "%s\n\n[Memory & Epistemic Context]:\n%s",
                 profile->system_prompt[0] ? profile->system_prompt : "You are BitchX AI.",
                 memory_context);
    } else {
        snprintf(sys_full, sizeof(sys_full), "%s",
                 profile->system_prompt[0] ? profile->system_prompt : "You are BitchX AI.");
    }
    json_escape(sys_full, esc_sys, sizeof(esc_sys));

    char esc_prompt[4096];
    char user_full[4096];
    snprintf(user_full, sizeof(user_full), "[%s in %s]: %s",
             nick ? nick : "user", channel ? channel : "query", prompt);
    json_escape(user_full, esc_prompt, sizeof(esc_prompt));

    char payload[16384];
    snprintf(payload, sizeof(payload),
             "{"
             "\"model\":\"%s\","
             "\"messages\":["
             "{\"role\":\"system\",\"content\":\"%s\"},"
             "{\"role\":\"user\",\"content\":\"%s\"}"
             "],"
             "\"temperature\":0.7"
             "}",
             profile->chat_model[0] ? profile->chat_model : "gpt-4o",
             esc_sys,
             esc_prompt);

    return post_json_completion(url, key, payload, resp);
}
