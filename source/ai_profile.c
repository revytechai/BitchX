/*
 * AI Profile & Session Management Implementation for BitchX (REVYTECH Edition)
 */

#include "ai_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

/* CPD-OFF -- preset catalog configuration structures */
/* Built-in Preset Catalog */
static const AIProfilePreset PRESET_CATALOG[] = {
    {
        .preset_name = "opencode",
        .description = "OpenCode / LiteLLM Proxy (OpenAI, Anthropic, Gemini, Ollama)",
        .chat_model = "gpt-4o",
        .api_endpoint = "http://127.0.0.1:4000/v1",
        .api_key_env = "OPENCODE_API_KEY",
        .memory_type = AI_MEMORY_EXTERNAL_HONCHO,
        .memory_model = "gpt-4o-mini",
        .system1_model = "gpt-4o-mini",
        .honcho_endpoint = "http://127.0.0.1:8000",
        .honcho_workspace = "bitchx"
    },
    {
        .preset_name = "cloudbsd",
        .description = "CloudBSD MCP Gateway (Gemini 2.5 Pro + Honcho Memory)",
        .chat_model = "gemini-2.5-pro",
        .api_endpoint = "https://mcp-gateway.cloudbsd.org/v1",
        .api_key_env = "CLOUDBD_API_KEY",
        .memory_type = AI_MEMORY_EXTERNAL_HONCHO,
        .memory_model = "gemini-2.5-flash",
        .system1_model = "gemini-2.5-flash",
        .honcho_endpoint = "https://mcp-gateway.cloudbsd.org/mcp",
        .honcho_workspace = "revytech"
    },
    {
        .preset_name = "ollama",
        .description = "Ollama Local Daemon (Private SQLite Internal Memory)",
        .chat_model = "llama3.2",
        .api_endpoint = "http://127.0.0.1:11434/v1",
        .api_key_env = "",
        .memory_type = AI_MEMORY_INTERNAL,
        .memory_model = "llama3.2:1b",
        .system1_model = "llama3.2:1b",
        .honcho_endpoint = "",
        .honcho_workspace = ""
    },
    {
        .preset_name = "openai",
        .description = "OpenAI Direct API (GPT-4o + Internal Memory)",
        .chat_model = "gpt-4o",
        .api_endpoint = "https://api.openai.com/v1",
        .api_key_env = "OPENAI_API_KEY",
        .memory_type = AI_MEMORY_INTERNAL,
        .memory_model = "gpt-4o-mini",
        .system1_model = "gpt-4o-mini",
        .honcho_endpoint = "",
        .honcho_workspace = ""
    },
    {
        .preset_name = "anthropic",
        .description = "Anthropic Direct API (Claude 3.5 Sonnet + Internal SQLite)",
        .chat_model = "claude-3-5-sonnet-latest",
        .api_endpoint = "https://api.anthropic.com/v1",
        .api_key_env = "ANTHROPIC_API_KEY",
        .memory_type = AI_MEMORY_INTERNAL,
        .memory_model = "claude-3-5-haiku-latest",
        .system1_model = "claude-3-5-haiku-latest",
        .honcho_endpoint = "",
        .honcho_workspace = ""
    },
    {
        .preset_name = "minimax",
        .description = "MiniMax API (MiniMax-Text-01 + Internal SQLite)",
        .chat_model = "MiniMax-Text-01",
        .api_endpoint = "https://api.minimax.chat/v1",
        .api_key_env = "MINIMAX_API_KEY",
        .memory_type = AI_MEMORY_INTERNAL,
        .memory_model = "MiniMax-Text-01",
        .system1_model = "MiniMax-Text-01",
        .honcho_endpoint = "",
        .honcho_workspace = ""
    }
};
/* CPD-ON */

static const int PRESET_CATALOG_COUNT = sizeof(PRESET_CATALOG) / sizeof(PRESET_CATALOG[0]);

int ai_profile_get_presets_count(void) {
    return PRESET_CATALOG_COUNT;
}

const AIProfilePreset *ai_profile_get_preset(int index) {
    if (index < 0 || index >= PRESET_CATALOG_COUNT) return NULL;
    return &PRESET_CATALOG[index];
}

int ai_profile_apply_preset(AIProfile *profile, int preset_index, const char *profile_name) {
    if (!profile || preset_index < 0 || preset_index >= PRESET_CATALOG_COUNT) return -1;
    const AIProfilePreset *p = &PRESET_CATALOG[preset_index];

    memset(profile, 0, sizeof(AIProfile));
    if (profile_name && *profile_name) {
        snprintf(profile->name, sizeof(profile->name), "%s", profile_name);
    } else {
        snprintf(profile->name, sizeof(profile->name), "%s", p->preset_name);
    }

    snprintf(profile->chat_model, sizeof(profile->chat_model), "%s", p->chat_model);
    snprintf(profile->api_endpoint, sizeof(profile->api_endpoint), "%s", p->api_endpoint);
    snprintf(profile->api_key_env, sizeof(profile->api_key_env), "%s", p->api_key_env);
    profile->memory_type = p->memory_type;
    snprintf(profile->memory_model, sizeof(profile->memory_model), "%s", p->memory_model);

    profile->trigger_mode = AI_TRIGGER_DEFAULT | AI_TRIGGER_AUTONOMOUS;
    snprintf(profile->system_prompt, sizeof(profile->system_prompt),
             "You are BitchX AI, a sharp, helpful, cyberpunk IRC assistant built for power users.");

    if (p->memory_type == AI_MEMORY_EXTERNAL_HONCHO) {
        snprintf(profile->honcho_config.workspace, sizeof(profile->honcho_config.workspace), "%s", p->honcho_workspace);
        snprintf(profile->honcho_config.endpoint, sizeof(profile->honcho_config.endpoint), "%s", p->honcho_endpoint);
        snprintf(profile->honcho_config.api_key_env, sizeof(profile->honcho_config.api_key_env), "%s", p->api_key_env);
        profile->honcho_config.auto_map_channel_session = 1;
        profile->honcho_config.auto_map_nick_peer = 1;
    } else {
        profile->internal_config.backend = ORM_BACKEND_SQLITE;
        snprintf(profile->internal_config.db_path, sizeof(profile->internal_config.db_path), AI_DEFAULT_SQLITE_PATH);
        snprintf(profile->internal_config.connection_string, sizeof(profile->internal_config.connection_string), AI_DEFAULT_SQLITE_PATH);
        snprintf(profile->internal_config.system1_model, sizeof(profile->internal_config.system1_model), "%s", p->system1_model);
        profile->internal_config.max_context_tokens = 4096;
    }

    profile->created_at = time(NULL);
    profile->updated_at = profile->created_at;
    return 0;
}

char *ai_expand_path(const char *path, char *buf, size_t buflen) {
    if (!path || !buf || buflen == 0) return NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(buf, buflen, "%s%s", home, path + 1);
    } else {
        snprintf(buf, buflen, "%s", path);
    }
    return buf;
}

static void ensure_parent_dir(const char *filepath) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", filepath);
    char *p = strrchr(dir, '/');
    if (p) {
        *p = '\0';
        mkdir(dir, 0700);
    }
}

void ai_profile_mgr_init(AIProfileManager *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(AIProfileManager));
}

int ai_profile_add(AIProfileManager *mgr, const AIProfile *profile) {
    if (!mgr || !profile || !profile->name[0]) return -1;
    if (mgr->count >= AI_MAX_PROFILES) return -2;

    for (int i = 0; i < mgr->count; i++) {
        if (!strcmp(mgr->profiles[i].name, profile->name)) {
            return -3;
        }
    }

    AIProfile *dest = &mgr->profiles[mgr->count];
    memcpy(dest, profile, sizeof(AIProfile));
    dest->created_at = time(NULL);
    dest->updated_at = dest->created_at;

    if (dest->trigger_mode == 0) {
        dest->trigger_mode = AI_TRIGGER_DEFAULT;
    }

    if (mgr->count == 0 || dest->is_active) {
        snprintf(mgr->active_profile, sizeof(mgr->active_profile), "%s", dest->name);
        dest->is_active = 1;
    }

    mgr->count++;
    return 0;
}

int ai_profile_update(AIProfileManager *mgr, const char *name, const AIProfile *profile) {
    if (!mgr || !name || !profile) return -1;
    for (int i = 0; i < mgr->count; i++) {
        if (!strcmp(mgr->profiles[i].name, name)) {
            time_t created = mgr->profiles[i].created_at;
            memcpy(&mgr->profiles[i], profile, sizeof(AIProfile));
            mgr->profiles[i].created_at = created;
            mgr->profiles[i].updated_at = time(NULL);
            if (profile->is_active) {
                snprintf(mgr->active_profile, sizeof(mgr->active_profile), "%s", name);
            }
            return 0;
        }
    }
    return -2;
}

int ai_profile_delete(AIProfileManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->count; i++) {
        if (!strcmp(mgr->profiles[i].name, name)) {
            for (int j = i; j < mgr->count - 1; j++) {
                mgr->profiles[j] = mgr->profiles[j + 1];
            }
            mgr->count--;
            memset(&mgr->profiles[mgr->count], 0, sizeof(AIProfile));

            if (!strcmp(mgr->active_profile, name)) {
                if (mgr->count > 0) {
                    snprintf(mgr->active_profile, sizeof(mgr->active_profile), "%s", mgr->profiles[0].name);
                    mgr->profiles[0].is_active = 1;
                } else {
                    mgr->active_profile[0] = '\0';
                }
            }
            return 0;
        }
    }
    return -2;
}

AIProfile *ai_profile_get(AIProfileManager *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->count; i++) {
        if (!strcmp(mgr->profiles[i].name, name)) {
            return &mgr->profiles[i];
        }
    }
    return NULL;
}

AIProfile *ai_profile_get_active(AIProfileManager *mgr) {
    if (!mgr || mgr->count == 0) return NULL;
    if (mgr->active_profile[0]) {
        AIProfile *p = ai_profile_get(mgr, mgr->active_profile);
        if (p) return p;
    }
    return &mgr->profiles[0];
}

AIProfile *ai_profile_get_for_channel(AIProfileManager *mgr, const char *channel) {
    if (!mgr || mgr->count == 0) return NULL;
    if (channel && *channel) {
        for (int i = 0; i < mgr->count; i++) {
            for (int c = 0; c < mgr->profiles[i].num_channels; c++) {
                if (!strcasecmp(mgr->profiles[i].channels[c], channel)) {
                    return &mgr->profiles[i];
                }
            }
        }
    }
    return ai_profile_get_active(mgr);
}

int ai_profile_set_active(AIProfileManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    AIProfile *p = ai_profile_get(mgr, name);
    if (!p) return -2;

    for (int i = 0; i < mgr->count; i++) {
        mgr->profiles[i].is_active = 0;
    }
    p->is_active = 1;
    snprintf(mgr->active_profile, sizeof(mgr->active_profile), "%s", name);
    return 0;
}

int ai_profile_add_channel(AIProfile *profile, const char *channel) {
    if (!profile || !channel || !*channel) return -1;
    if (profile->num_channels >= AI_MAX_CHANNELS) return -2;

    for (int i = 0; i < profile->num_channels; i++) {
        if (!strcasecmp(profile->channels[i], channel)) {
            return 0;
        }
    }
    snprintf(profile->channels[profile->num_channels], AI_MAX_CHANNEL_LEN, "%s", channel);
    profile->num_channels++;
    return 0;
}

int ai_profile_remove_channel(AIProfile *profile, const char *channel) {
    if (!profile || !channel) return -1;
    for (int i = 0; i < profile->num_channels; i++) {
        if (!strcasecmp(profile->channels[i], channel)) {
            for (int j = i; j < profile->num_channels - 1; j++) {
                snprintf(profile->channels[j], AI_MAX_CHANNEL_LEN, "%s", profile->channels[j + 1]);
            }
            profile->num_channels--;
            return 0;
        }
    }
    return -2;
}

/* --- JSON Parsing Helpers --- */
static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int extract_json_string(const char *json, const char *key, char *dest, size_t maxlen) {
    if (!json || !key || !dest || maxlen == 0) return -1;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    p = skip_ws(p);
    if (*p != ':') return -1;
    p = skip_ws(p + 1);
    if (*p != '\"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '\"' && i < maxlen - 1) {
        if (*p == '\\' && *(p + 1)) p++;
        dest[i++] = *p++;
    }
    dest[i] = '\0';
    return 0;
}

static int extract_json_int(const char *json, const char *key, int *dest) {
    if (!json || !key || !dest) return -1;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    p = skip_ws(p);
    if (*p != ':') return -1;
    p = skip_ws(p + 1);
    *dest = atoi(p);
    return 0;
}

/* CPD-OFF -- JSON serialization and deserialization routines */
/* --- Serialization and Persistence --- */
int ai_profile_save_json(const AIProfileManager *mgr, const char *filepath) {
    if (!mgr || !filepath) return -1;
    char real_path[512];
    ai_expand_path(filepath, real_path, sizeof(real_path));
    ensure_parent_dir(real_path);

    FILE *f = fopen(real_path, "w");
    if (!f) return -1;

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 2,\n");
    fprintf(f, "  \"active_profile\": \"%s\",\n", mgr->active_profile);
    fprintf(f, "  \"profiles\": [\n");

    for (int i = 0; i < mgr->count; i++) {
        const AIProfile *p = &mgr->profiles[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", p->name);
        fprintf(f, "      \"chat_model\": \"%s\",\n", p->chat_model);
        fprintf(f, "      \"api_endpoint\": \"%s\",\n", p->api_endpoint);
        fprintf(f, "      \"api_key_env\": \"%s\",\n", p->api_key_env);
        fprintf(f, "      \"memory_type\": %d,\n", (int)p->memory_type);
        fprintf(f, "      \"memory_model\": \"%s\",\n", p->memory_model);
        fprintf(f, "      \"trigger_mode\": %d,\n", p->trigger_mode);
        fprintf(f, "      \"system_prompt\": \"%s\",\n", p->system_prompt);
        fprintf(f, "      \"is_active\": %d,\n", p->is_active);

        /* Honcho sub-config */
        fprintf(f, "      \"honcho\": {\n");
        fprintf(f, "        \"workspace\": \"%s\",\n", p->honcho_config.workspace);
        fprintf(f, "        \"peer_id\": \"%s\",\n", p->honcho_config.peer_id);
        fprintf(f, "        \"session_id\": \"%s\",\n", p->honcho_config.session_id);
        fprintf(f, "        \"endpoint\": \"%s\",\n", p->honcho_config.endpoint);
        fprintf(f, "        \"api_key_env\": \"%s\",\n", p->honcho_config.api_key_env);
        fprintf(f, "        \"auto_map_channel_session\": %d,\n", p->honcho_config.auto_map_channel_session);
        fprintf(f, "        \"auto_map_nick_peer\": %d\n", p->honcho_config.auto_map_nick_peer);
        fprintf(f, "      },\n");

        /* Internal sub-config */
        fprintf(f, "      \"internal\": {\n");
        fprintf(f, "        \"backend\": %d,\n", (int)p->internal_config.backend);
        fprintf(f, "        \"db_path\": \"%s\",\n", p->internal_config.db_path);
        fprintf(f, "        \"connection_string\": \"%s\",\n", p->internal_config.connection_string);
        fprintf(f, "        \"system1_model\": \"%s\",\n", p->internal_config.system1_model);
        fprintf(f, "        \"max_context_tokens\": %d\n", p->internal_config.max_context_tokens);
        fprintf(f, "      },\n");

        /* Channels */
        fprintf(f, "      \"channels\": [");
        for (int c = 0; c < p->num_channels; c++) {
            fprintf(f, "\"%s\"%s", p->channels[c], (c < p->num_channels - 1) ? ", " : "");
        }
        fprintf(f, "]\n");

        fprintf(f, "    }%s\n", (i < mgr->count - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

/* #lizard forgives -- JSON parser for profile persistence */
int ai_profile_load_json(AIProfileManager *mgr, const char *filepath) {
    if (!mgr || !filepath) return -1;
    char real_path[512];
    ai_expand_path(filepath, real_path, sizeof(real_path));

    FILE *f = fopen(real_path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 2 * 1024 * 1024) {
        fclose(f);
        return -2;
    }

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return -3;
    }
    size_t read_bytes = fread(buf, 1, len, f);
    buf[read_bytes] = '\0';
    fclose(f);

    ai_profile_mgr_init(mgr);
    extract_json_string(buf, "active_profile", mgr->active_profile, sizeof(mgr->active_profile));

    const char *p = strstr(buf, "\"profiles\"");
    if (!p) {
        free(buf);
        return -4;
    }
    p = strchr(p, '[');
    if (!p) {
        free(buf);
        return -4;
    }

    while ((p = strchr(p, '{')) != NULL) {
        int depth = 0;
        const char *cursor = p;
        const char *end_obj = NULL;
        while (*cursor) {
            if (*cursor == '{') depth++;
            else if (*cursor == '}') {
                depth--;
                if (depth == 0) {
                    end_obj = cursor;
                    break;
                }
            }
            cursor++;
        }
        if (!end_obj) break;

        size_t block_len = end_obj - p + 1;
        char *block = (char *)malloc(block_len + 1);
        if (!block) break;
        memcpy(block, p, block_len);
        block[block_len] = '\0';

        AIProfile prof;
        memset(&prof, 0, sizeof(AIProfile));

        extract_json_string(block, "name", prof.name, sizeof(prof.name));
        extract_json_string(block, "chat_model", prof.chat_model, sizeof(prof.chat_model));
        extract_json_string(block, "api_endpoint", prof.api_endpoint, sizeof(prof.api_endpoint));
        extract_json_string(block, "api_key_env", prof.api_key_env, sizeof(prof.api_key_env));
        extract_json_string(block, "memory_model", prof.memory_model, sizeof(prof.memory_model));
        extract_json_string(block, "system_prompt", prof.system_prompt, sizeof(prof.system_prompt));

        int int_val = 0;
        if (extract_json_int(block, "memory_type", &int_val) == 0)
            prof.memory_type = (AIMemoryType)int_val;
        if (extract_json_int(block, "trigger_mode", &int_val) == 0)
            prof.trigger_mode = int_val;
        else
            prof.trigger_mode = AI_TRIGGER_DEFAULT;
        if (extract_json_int(block, "is_active", &int_val) == 0)
            prof.is_active = int_val;

        /* Sub-configs */
        const char *honcho_p = strstr(block, "\"honcho\"");
        if (honcho_p) {
            extract_json_string(honcho_p, "workspace", prof.honcho_config.workspace, sizeof(prof.honcho_config.workspace));
            extract_json_string(honcho_p, "peer_id", prof.honcho_config.peer_id, sizeof(prof.honcho_config.peer_id));
            extract_json_string(honcho_p, "session_id", prof.honcho_config.session_id, sizeof(prof.honcho_config.session_id));
            extract_json_string(honcho_p, "endpoint", prof.honcho_config.endpoint, sizeof(prof.honcho_config.endpoint));
            extract_json_string(honcho_p, "api_key_env", prof.honcho_config.api_key_env, sizeof(prof.honcho_config.api_key_env));
            if (extract_json_int(honcho_p, "auto_map_channel_session", &int_val) == 0)
                prof.honcho_config.auto_map_channel_session = int_val;
            else
                prof.honcho_config.auto_map_channel_session = 1;
            if (extract_json_int(honcho_p, "auto_map_nick_peer", &int_val) == 0)
                prof.honcho_config.auto_map_nick_peer = int_val;
            else
                prof.honcho_config.auto_map_nick_peer = 1;
        }

        const char *internal_p = strstr(block, "\"internal\"");
        if (internal_p) {
            if (extract_json_int(internal_p, "backend", &int_val) == 0)
                prof.internal_config.backend = (AIORMBackend)int_val;
            extract_json_string(internal_p, "db_path", prof.internal_config.db_path, sizeof(prof.internal_config.db_path));
            extract_json_string(internal_p, "connection_string", prof.internal_config.connection_string, sizeof(prof.internal_config.connection_string));
            extract_json_string(internal_p, "system1_model", prof.internal_config.system1_model, sizeof(prof.internal_config.system1_model));
            extract_json_int(internal_p, "max_context_tokens", &prof.internal_config.max_context_tokens);
        }

        /* Channel array */
        const char *chan_p = strstr(block, "\"channels\"");
        if (chan_p) {
            const char *arr_start = strchr(chan_p, '[');
            const char *arr_end = strchr(chan_p, ']');
            if (arr_start && arr_end && arr_end > arr_start) {
                const char *cptr = arr_start;
                while (cptr < arr_end && prof.num_channels < AI_MAX_CHANNELS) {
                    cptr = strchr(cptr, '\"');
                    if (!cptr || cptr >= arr_end) break;
                    cptr++;
                    const char *q2 = strchr(cptr, '\"');
                    if (!q2 || q2 > arr_end) break;
                    size_t ch_len = q2 - cptr;
                    if (ch_len > 0 && ch_len < AI_MAX_CHANNEL_LEN) {
                        memcpy(prof.channels[prof.num_channels], cptr, ch_len);
                        prof.channels[prof.num_channels][ch_len] = '\0';
                        prof.num_channels++;
                    }
                    cptr = q2 + 1;
                }
            }
        }

        if (prof.name[0]) {
            ai_profile_add(mgr, &prof);
        }

        free(block);
        p = end_obj + 1;
    }

    free(buf);
    return 0;
}

int ai_profile_backup(const AIProfileManager *mgr, const char *backup_path) {
    if (!mgr) return -1;
    char path[512];
    if (backup_path && *backup_path) {
        ai_expand_path(backup_path, path, sizeof(path));
    } else {
        time_t now = time(NULL);
        char home_path[512];
        ai_expand_path("~/.BitchX", home_path, sizeof(home_path));
        snprintf(path, sizeof(path), "%s/ai_profiles_backup_%lu.json", home_path, (unsigned long)now);
    }
    return ai_profile_save_json(mgr, path);
}

int ai_profile_restore(AIProfileManager *mgr, const char *backup_path) {
    if (!mgr || !backup_path) return -1;
    char path[512];
    ai_expand_path(backup_path, path, sizeof(path));
    return ai_profile_load_json(mgr, path);
}

/* --- Session State Management --- */
void ai_session_state_init(AISessionState *session) {
    if (!session) return;
    memset(session, 0, sizeof(AISessionState));
    session->default_port = 6697;
    session->use_ssl = 1;
}

int ai_session_save(const AISessionState *session, const char *filepath) {
    if (!session || !filepath) return -1;
    char path[512];
    ai_expand_path(filepath, path, sizeof(path));
    ensure_parent_dir(path);

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"nickname\": \"%s\",\n", session->nickname);
    fprintf(f, "  \"realname\": \"%s\",\n", session->realname);
    fprintf(f, "  \"default_server\": \"%s\",\n", session->default_server);
    fprintf(f, "  \"default_port\": %d,\n", session->default_port);
    fprintf(f, "  \"use_ssl\": %d,\n", session->use_ssl);
    fprintf(f, "  \"active_ai_profile\": \"%s\",\n", session->active_ai_profile);
    fprintf(f, "  \"window_split_count\": %d,\n", session->window_split_count);
    fprintf(f, "  \"saved_at\": %lu,\n", (unsigned long)time(NULL));

    fprintf(f, "  \"joined_channels\": [");
    for (int i = 0; i < session->num_joined_channels; i++) {
        fprintf(f, "\"%s\"%s", session->joined_channels[i], (i < session->num_joined_channels - 1) ? ", " : "");
    }
    fprintf(f, "]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

int ai_session_load(AISessionState *session, const char *filepath) {
    if (!session || !filepath) return -1;
    char path[512];
    ai_expand_path(filepath, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > 1024 * 1024) {
        fclose(f);
        return -2;
    }

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return -3;
    }
    size_t rb = fread(buf, 1, len, f);
    buf[rb] = '\0';
    fclose(f);

    ai_session_state_init(session);
    extract_json_string(buf, "nickname", session->nickname, sizeof(session->nickname));
    extract_json_string(buf, "realname", session->realname, sizeof(session->realname));
    extract_json_string(buf, "default_server", session->default_server, sizeof(session->default_server));
    extract_json_string(buf, "active_ai_profile", session->active_ai_profile, sizeof(session->active_ai_profile));
    extract_json_int(buf, "default_port", &session->default_port);
    extract_json_int(buf, "use_ssl", &session->use_ssl);
    extract_json_int(buf, "window_split_count", &session->window_split_count);

    const char *chan_p = strstr(buf, "\"joined_channels\"");
    if (chan_p) {
        const char *arr_start = strchr(chan_p, '[');
        const char *arr_end = strchr(chan_p, ']');
        if (arr_start && arr_end && arr_end > arr_start) {
            const char *cptr = arr_start;
            while (cptr < arr_end && session->num_joined_channels < AI_MAX_CHANNELS) {
                cptr = strchr(cptr, '\"');
                if (!cptr || cptr >= arr_end) break;
                cptr++;
                const char *q2 = strchr(cptr, '\"');
                if (!q2 || q2 > arr_end) break;
                size_t ch_len = q2 - cptr;
                if (ch_len > 0 && ch_len < AI_MAX_CHANNEL_LEN) {
                    memcpy(session->joined_channels[session->num_joined_channels], cptr, ch_len);
                    session->joined_channels[session->num_joined_channels][ch_len] = '\0';
                    session->num_joined_channels++;
                }
                cptr = q2 + 1;
            }
        }
    }

    free(buf);
    return 0;
}
/* CPD-ON */
