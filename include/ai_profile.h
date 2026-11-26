/*
 * AI Profile & Session Management for BitchX (REVYTECH Edition)
 * Supports multi-provider routing, internal ORM store, external Honcho memory,
 * dynamic user presets, and complete userspace session persistence.
 */

#ifndef _AI_PROFILE_H_
#define _AI_PROFILE_H_

#include <stddef.h>
#include <time.h>

#define AI_MAX_NAME_LEN        64
#define AI_MAX_MODEL_LEN       128
#define AI_MAX_ENDPOINT_LEN    256
#define AI_MAX_KEY_ENV_LEN     64
#define AI_MAX_CHANNELS        32
#define AI_MAX_CHANNEL_LEN     64
#define AI_MAX_PROFILES        32
#define AI_MAX_PRESETS         32
#define AI_MAX_PROMPT_LEN      1024

#define AI_DEFAULT_CONFIG_PATH   "~/.BitchX/ai_profiles.json"
#define AI_DEFAULT_PRESETS_PATH  "~/.BitchX/ai_presets.json"
#define AI_DEFAULT_SESSION_PATH  "~/.BitchX/session.json"
#define AI_DEFAULT_SQLITE_PATH   "~/.BitchX/ai_memory.db"

/* Interaction Trigger Modes (bitmask) */
#define AI_TRIGGER_COMMAND     (1 << 0) /* Respond to /AI and /ASK commands */
#define AI_TRIGGER_MENTION     (1 << 1) /* Respond to @nick or botnick: in channel */
#define AI_TRIGGER_QUERY       (1 << 2) /* Respond to private query messages */
#define AI_TRIGGER_AUTONOMOUS  (1 << 3) /* System 1 evaluates incoming chat to chime in */
#define AI_TRIGGER_DEFAULT     (AI_TRIGGER_COMMAND | AI_TRIGGER_MENTION | AI_TRIGGER_QUERY)

/* Memory Store Type */
typedef enum {
    AI_MEMORY_INTERNAL = 0,
    AI_MEMORY_EXTERNAL_HONCHO = 1
} AIMemoryType;

/* ORM Backend Type for Internal Store */
typedef enum {
    ORM_BACKEND_SQLITE = 0,
    ORM_BACKEND_POSTGRES = 1
} AIORMBackend;

/* External Honcho Configuration */
typedef struct {
    char workspace[AI_MAX_NAME_LEN];
    char peer_id[AI_MAX_NAME_LEN];          /* Default peer or fallback */
    char session_id[AI_MAX_NAME_LEN];       /* Default session or fallback */
    char endpoint[AI_MAX_ENDPOINT_LEN];
    char api_key_env[AI_MAX_KEY_ENV_LEN];
    int auto_map_channel_session;           /* 1 = #channel -> session_id */
    int auto_map_nick_peer;                 /* 1 = nick -> peer_id */
} AIHonchoConfig;

/* Internal Store ORM Configuration */
typedef struct {
    AIORMBackend backend;
    char db_path[AI_MAX_ENDPOINT_LEN];
    char connection_string[AI_MAX_ENDPOINT_LEN];
    char system1_model[AI_MAX_MODEL_LEN];   /* Optional fast reactive model */
    int max_context_tokens;
} AIInternalStoreConfig;

/* AI Profile Structure */
typedef struct {
    char name[AI_MAX_NAME_LEN];
    char chat_model[AI_MAX_MODEL_LEN];
    char api_endpoint[AI_MAX_ENDPOINT_LEN];
    char api_key_env[AI_MAX_KEY_ENV_LEN];

    AIMemoryType memory_type;
    char memory_model[AI_MAX_MODEL_LEN];    /* Model for memory distillation */

    AIHonchoConfig honcho_config;
    AIInternalStoreConfig internal_config;

    char channels[AI_MAX_CHANNELS][AI_MAX_CHANNEL_LEN];
    int num_channels;

    int trigger_mode;                       /* Bitmask of AI_TRIGGER_* */
    char system_prompt[AI_MAX_PROMPT_LEN];

    int is_active;
    time_t created_at;
    time_t updated_at;
} AIProfile;

/* Dynamic Profile Preset Definition (Loaded from ~/.BitchX/ai_presets.json) */
typedef struct {
    char preset_name[AI_MAX_NAME_LEN];
    char description[128];
    char chat_model[AI_MAX_MODEL_LEN];
    char api_endpoint[AI_MAX_ENDPOINT_LEN];
    char api_key_env[AI_MAX_KEY_ENV_LEN];
    AIMemoryType memory_type;
    char memory_model[AI_MAX_MODEL_LEN];
    char system1_model[AI_MAX_MODEL_LEN];
    char honcho_endpoint[AI_MAX_ENDPOINT_LEN];
    char honcho_workspace[AI_MAX_NAME_LEN];
} AIProfilePreset;

/* Profiles Container */
typedef struct {
    AIProfile profiles[AI_MAX_PROFILES];
    int count;
    char active_profile[AI_MAX_NAME_LEN];
} AIProfileManager;

/* Userspace Session Persistence Structure */
typedef struct {
    char nickname[64];
    char realname[128];
    char default_server[128];
    int default_port;
    int use_ssl;

    char joined_channels[AI_MAX_CHANNELS][AI_MAX_CHANNEL_LEN];
    int num_joined_channels;

    char active_ai_profile[AI_MAX_NAME_LEN];
    int window_split_count;
    time_t saved_at;
} AISessionState;

/* --- Dynamic Preset Catalog API --- */
int                     ai_presets_init(void);
int                     ai_presets_load_json(const char *filepath);
int                     ai_presets_save_json(const char *filepath);
int                     ai_profile_get_presets_count(void);
const AIProfilePreset  *ai_profile_get_preset(int index);
int                     ai_profile_apply_preset(AIProfile *profile, int preset_index, const char *profile_name);

/* --- Profile Manager API --- */
void       ai_profile_mgr_init(AIProfileManager *mgr);
int        ai_profile_add(AIProfileManager *mgr, const AIProfile *profile);
int        ai_profile_update(AIProfileManager *mgr, const char *name, const AIProfile *profile);
int        ai_profile_delete(AIProfileManager *mgr, const char *name);
AIProfile *ai_profile_get(AIProfileManager *mgr, const char *name);
AIProfile *ai_profile_get_active(AIProfileManager *mgr);
int        ai_profile_set_active(AIProfileManager *mgr, const char *name);
AIProfile *ai_profile_get_for_channel(AIProfileManager *mgr, const char *channel);

int        ai_profile_add_channel(AIProfile *profile, const char *channel);
int        ai_profile_remove_channel(AIProfile *profile, const char *channel);

/* --- Persistence and Serialization --- */
int        ai_profile_save_json(const AIProfileManager *mgr, const char *filepath);
int        ai_profile_load_json(AIProfileManager *mgr, const char *filepath);
int        ai_profile_backup(const AIProfileManager *mgr, const char *backup_path);
int        ai_profile_restore(AIProfileManager *mgr, const char *backup_path);

/* --- Userspace Session API --- */
void       ai_session_state_init(AISessionState *session);
int        ai_session_save(const AISessionState *session, const char *filepath);
int        ai_session_load(AISessionState *session, const char *filepath);

/* Helper */
char      *ai_expand_path(const char *path, char *buf, size_t buflen);

#endif /* _AI_PROFILE_H_ */
