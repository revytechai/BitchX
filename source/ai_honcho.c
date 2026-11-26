/*
 * Honcho HTTP Client Implementation for BitchX AI Subsystem (libcurl backed)
 */

#include "ai_honcho.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

static int g_curl_initialized = 0;

int ai_honcho_global_init(void) {
    if (!g_curl_initialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {
            g_curl_initialized = 1;
            return 0;
        }
        return -1;
    }
    return 0;
}

void ai_honcho_global_cleanup(void) {
    if (g_curl_initialized) {
        curl_global_cleanup();
        g_curl_initialized = 0;
    }
}

/* Sanitize IRC channel/nick names into clean alphanumeric/hyphen identifiers */
static void sanitize_identifier(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_len - 1; i++) {
        char c = in[i];
        if (c == '#' || c == '&' || c == '+' || c == '!') {
            /* Skip channel prefix if leading, or replace with dash */
            if (j > 0 && out[j - 1] != '-') out[j++] = '-';
        } else if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            out[j++] = (char)tolower((unsigned char)c);
        } else {
            if (j > 0 && out[j - 1] != '-') out[j++] = '-';
        }
    }
    if (j == 0) {
        out[j++] = 'd';
        out[j++] = 'e';
        out[j++] = 'f';
    }
    out[j] = '\0';
}

void ai_honcho_resolve_ids(const AIHonchoConfig *cfg, const char *channel, const char *nick,
                           char *out_session_id, size_t session_buflen,
                           char *out_peer_id, size_t peer_buflen) {
    if (out_session_id && session_buflen > 0) {
        if (cfg && cfg->auto_map_channel_session && channel && *channel) {
            sanitize_identifier(channel, out_session_id, session_buflen);
        } else if (cfg && cfg->session_id[0]) {
            snprintf(out_session_id, session_buflen, "%s", cfg->session_id);
        } else {
            snprintf(out_session_id, session_buflen, "general");
        }
    }

    if (out_peer_id && peer_buflen > 0) {
        if (cfg && cfg->auto_map_nick_peer && nick && *nick) {
            sanitize_identifier(nick, out_peer_id, peer_buflen);
        } else if (cfg && cfg->peer_id[0]) {
            snprintf(out_peer_id, peer_buflen, "%s", cfg->peer_id);
        } else {
            snprintf(out_peer_id, peer_buflen, "user");
        }
    }
}

/* Callback for libcurl to write received chunks into buffer */
struct MemoryStruct {
    char *memory;
    size_t size;
    size_t max_size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    if (mem->size + realsize >= mem->max_size) {
        realsize = mem->max_size - mem->size - 1;
    }

    if (realsize > 0) {
        memcpy(&(mem->memory[mem->size]), contents, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }

    return size * nmemb;
}

/* CPD-OFF -- libcurl HTTP execution helper */
static int execute_http(const char *url, const char *auth_header, const char *post_data, AIHonchoResponse *resp) {
    if (!url || !resp) return -1;
    memset(resp, 0, sizeof(AIHonchoResponse));

    ai_honcho_global_init();

    CURL *curl = curl_easy_init();
    if (!curl) {
        resp->is_error = 1;
        snprintf(resp->error_msg, sizeof(resp->error_msg), "Failed to initialize CURL handle");
        return -1;
    }

    struct MemoryStruct chunk;
    chunk.memory = resp->raw_json;
    chunk.size = 0;
    chunk.max_size = sizeof(resp->raw_json);
    chunk.memory[0] = '\0';

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    if (auth_header && *auth_header) {
        char auth_buf[300];
        snprintf(auth_buf, sizeof(auth_buf), "Authorization: Bearer %s", auth_header);
        headers = curl_slist_append(headers, auth_buf);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp->is_error = 1;
        snprintf(resp->error_msg, sizeof(resp->error_msg), "CURL error: %s", curl_easy_strerror(res));
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->status_code);
        if (resp->status_code >= 400) {
            resp->is_error = 1;
            snprintf(resp->error_msg, sizeof(resp->error_msg), "HTTP error %ld", resp->status_code);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return resp->is_error ? -1 : 0;
}
/* CPD-ON */

static const char *get_api_key(const AIHonchoConfig *cfg) {
    if (!cfg || !cfg->api_key_env[0]) return NULL;
    return getenv(cfg->api_key_env);
}

int ai_honcho_get_peer_context(const AIHonchoConfig *cfg, const char *peer_id, AIHonchoResponse *resp) {
    if (!cfg || !resp) return -1;
    const char *endpoint = cfg->endpoint[0] ? cfg->endpoint : "http://localhost:8000";
    const char *workspace = cfg->workspace[0] ? cfg->workspace : "bitchx";
    const char *peer = (peer_id && *peer_id) ? peer_id : (cfg->peer_id[0] ? cfg->peer_id : "user");

    char url[512];
    snprintf(url, sizeof(url), "%s/workspaces/%s/peers/%s/card", endpoint, workspace, peer);

    return execute_http(url, get_api_key(cfg), NULL, resp);
}

int ai_honcho_get_session_context(const AIHonchoConfig *cfg, const char *session_id, AIHonchoResponse *resp) {
    if (!cfg || !resp) return -1;
    const char *endpoint = cfg->endpoint[0] ? cfg->endpoint : "http://localhost:8000";
    const char *workspace = cfg->workspace[0] ? cfg->workspace : "bitchx";
    const char *session = (session_id && *session_id) ? session_id : (cfg->session_id[0] ? cfg->session_id : "general");

    char url[512];
    snprintf(url, sizeof(url), "%s/workspaces/%s/sessions/%s/context", endpoint, workspace, session);

    return execute_http(url, get_api_key(cfg), NULL, resp);
}

/* CPD-OFF -- Honcho REST payload dispatch */
int ai_honcho_create_conclusion(const AIHonchoConfig *cfg, const char *session_id, const char *peer_id,
                                 const char *conclusion_text, AIHonchoResponse *resp) {
    if (!cfg || !conclusion_text || !resp) return -1;
    const char *endpoint = cfg->endpoint[0] ? cfg->endpoint : "http://localhost:8000";
    const char *workspace = cfg->workspace[0] ? cfg->workspace : "bitchx";
    const char *session = (session_id && *session_id) ? session_id : (cfg->session_id[0] ? cfg->session_id : "general");
    const char *peer = (peer_id && *peer_id) ? peer_id : (cfg->peer_id[0] ? cfg->peer_id : "user");

    char url[512];
    snprintf(url, sizeof(url), "%s/workspaces/%s/conclusions", endpoint, workspace);

    /* Construct payload */
    char payload[4096];
    snprintf(payload, sizeof(payload),
             "{\"session_id\":\"%s\",\"peer_id\":\"%s\",\"conclusion\":\"%s\"}",
             session, peer, conclusion_text);

    return execute_http(url, get_api_key(cfg), payload, resp);
}

int ai_honcho_add_message(const AIHonchoConfig *cfg, const char *session_id, const char *peer_id,
                           const char *role, const char *content, AIHonchoResponse *resp) {
    if (!cfg || !role || !content || !resp) return -1;
    const char *endpoint = cfg->endpoint[0] ? cfg->endpoint : "http://localhost:8000";
    const char *workspace = cfg->workspace[0] ? cfg->workspace : "bitchx";
    const char *session = (session_id && *session_id) ? session_id : (cfg->session_id[0] ? cfg->session_id : "general");
    const char *peer = (peer_id && *peer_id) ? peer_id : (cfg->peer_id[0] ? cfg->peer_id : "user");

    char url[512];
    snprintf(url, sizeof(url), "%s/workspaces/%s/sessions/%s/messages", endpoint, workspace, session);

    char payload[4096];
    snprintf(payload, sizeof(payload),
             "{\"peer_id\":\"%s\",\"role\":\"%s\",\"content\":\"%s\"}",
             peer, role, content);

    return execute_http(url, get_api_key(cfg), payload, resp);
}
/* CPD-ON */

int ai_honcho_ping(const AIHonchoConfig *cfg, AIHonchoResponse *resp) {
    if (!cfg || !resp) return -1;
    const char *endpoint = cfg->endpoint[0] ? cfg->endpoint : "http://localhost:8000";
    char url[512];
    snprintf(url, sizeof(url), "%s/health", endpoint);
    return execute_http(url, get_api_key(cfg), NULL, resp);
}
