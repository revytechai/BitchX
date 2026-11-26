/*
 * Database ORM Abstraction Layer for BitchX AI Subsystem
 * Provides a unified entity data-access interface with pluggable drivers
 * for SQLite and PostgreSQL.
 */

#ifndef _AI_ORM_H_
#define _AI_ORM_H_

#include <time.h>
#include <stddef.h>
#include "ai_profile.h"

#define AI_MAX_RECORD_CONTENT 2048
#define AI_MAX_TOPIC_LEN       128
#define AI_MAX_SUMMARY_LEN     1024

/* Entity: Chat Message Record */
typedef struct {
    long id;
    char profile_name[AI_MAX_NAME_LEN];
    char channel[AI_MAX_CHANNEL_LEN];
    char nick[64];
    char role[16]; /* "user", "assistant", "system" */
    char content[AI_MAX_RECORD_CONTENT];
    time_t created_at;
} AIMessageRecord;

/* Entity: Memory / Epistemic Record */
typedef struct {
    long id;
    char profile_name[AI_MAX_NAME_LEN];
    char topic[AI_MAX_TOPIC_LEN];
    char summary[AI_MAX_SUMMARY_LEN];
    double confidence;
    time_t created_at;
} AIMemoryRecord;

/* Abstract Database Connection Handle */
typedef struct AIORMHandle AIORMHandle;

/* Connection & Lifecycle */
AIORMHandle *ai_orm_connect(AIORMBackend backend, const char *connection_string);
int          ai_orm_disconnect(AIORMHandle *handle);
int          ai_orm_migrate_schema(AIORMHandle *handle);
AIORMBackend ai_orm_get_backend(const AIORMHandle *handle);

/* Message Entity CRUD */
int          ai_orm_save_message(AIORMHandle *handle, const AIMessageRecord *record);
int          ai_orm_get_messages(AIORMHandle *handle, const char *profile_name, const char *channel, int limit, AIMessageRecord *out_records, int max_records);

/* Memory Entity CRUD */
int          ai_orm_save_memory(AIORMHandle *handle, const AIMemoryRecord *record);
int          ai_orm_query_memories(AIORMHandle *handle, const char *profile_name, const char *topic, int limit, AIMemoryRecord *out_records, int max_records);

#endif /* _AI_ORM_H_ */
