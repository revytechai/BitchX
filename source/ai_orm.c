/*
 * Database ORM Abstraction Layer for BitchX AI Subsystem
 * Supports SQLite (embedded) and PostgreSQL (network) backends.
 */

#include "ai_orm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

struct AIORMHandle {
    AIORMBackend backend;
    sqlite3 *sqlite_db;
    void *pg_conn; /* Pluggable for libpq / PostgreSQL */
    char connection_string[512];
};

AIORMBackend ai_orm_get_backend(const AIORMHandle *handle) {
    if (!handle) return ORM_BACKEND_SQLITE;
    return handle->backend;
}

AIORMHandle *ai_orm_connect(AIORMBackend backend, const char *connection_string) {
    if (!connection_string || !*connection_string) return NULL;

    AIORMHandle *h = (AIORMHandle *)calloc(1, sizeof(AIORMHandle));
    if (!h) return NULL;

    h->backend = backend;
    snprintf(h->connection_string, sizeof(h->connection_string), "%s", connection_string);

    if (backend == ORM_BACKEND_SQLITE) {
        char expanded_path[512];
        ai_expand_path(connection_string, expanded_path, sizeof(expanded_path));

        /* Ensure directory exists */
        char dir[512];
        snprintf(dir, sizeof(dir), "%s", expanded_path);
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            char mkdir_cmd[600];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\" 2>/dev/null", dir);
            (void)system(mkdir_cmd);
        }

        int rc = sqlite3_open(expanded_path, &h->sqlite_db);
        if (rc != SQLITE_OK) {
            if (h->sqlite_db) sqlite3_close(h->sqlite_db);
            free(h);
            return NULL;
        }

        if (ai_orm_migrate_schema(h) != 0) {
            sqlite3_close(h->sqlite_db);
            free(h);
            return NULL;
        }
    } else {
        /* PostgreSQL driver placeholder / connection initialization */
        /* If configured for postgres, connection string holds URI postgresql://user:pass@host/db */
        h->pg_conn = NULL;
    }

    return h;
}

int ai_orm_disconnect(AIORMHandle *handle) {
    if (!handle) return -1;
    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        sqlite3_close(handle->sqlite_db);
        handle->sqlite_db = NULL;
    }
    free(handle);
    return 0;
}

int ai_orm_migrate_schema(AIORMHandle *handle) {
    if (!handle) return -1;

    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        const char *schema =
            "CREATE TABLE IF NOT EXISTS ai_messages ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  profile_name TEXT NOT NULL,"
            "  channel TEXT NOT NULL,"
            "  nick TEXT NOT NULL,"
            "  role TEXT NOT NULL,"
            "  content TEXT NOT NULL,"
            "  created_at INTEGER NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_msg_prof_chan ON ai_messages(profile_name, channel);"
            "CREATE TABLE IF NOT EXISTS ai_memories ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  profile_name TEXT NOT NULL,"
            "  topic TEXT NOT NULL,"
            "  summary TEXT NOT NULL,"
            "  confidence REAL NOT NULL,"
            "  created_at INTEGER NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_mem_topic ON ai_memories(profile_name, topic);";

        char *err_msg = NULL;
        int rc = sqlite3_exec(handle->sqlite_db, schema, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) sqlite3_free(err_msg);
            return -2;
        }
        return 0;
    }
    return 0;
}

/* CPD-OFF -- SQLite parameter binding and entity extraction */
int ai_orm_save_message(AIORMHandle *handle, const AIMessageRecord *record) {
    if (!handle || !record) return -1;

    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        const char *sql = "INSERT INTO ai_messages (profile_name, channel, nick, role, content, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(handle->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            return -2;
        }

        sqlite3_bind_text(stmt, 1, record->profile_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, record->channel, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, record->nick, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, record->role, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, record->content, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, record->created_at ? record->created_at : time(NULL));

        int step_rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (step_rc == SQLITE_DONE) ? 0 : -3;
    }
    return -1;
}

int ai_orm_get_messages(AIORMHandle *handle, const char *profile_name, const char *channel, int limit, AIMessageRecord *out_records, int max_records) {
    if (!handle || !out_records || max_records <= 0) return 0;
    if (limit <= 0 || limit > max_records) limit = max_records;

    int count = 0;
    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        const char *sql = "SELECT id, profile_name, channel, nick, role, content, created_at "
                          "FROM ai_messages WHERE profile_name = ? AND channel = ? "
                          "ORDER BY id DESC LIMIT ?;";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(handle->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            return 0;
        }

        sqlite3_bind_text(stmt, 1, profile_name ? profile_name : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, channel ? channel : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW && count < max_records) {
            AIMessageRecord *r = &out_records[count];
            r->id = (long)sqlite3_column_int64(stmt, 0);
            snprintf(r->profile_name, sizeof(r->profile_name), "%s", (const char *)sqlite3_column_text(stmt, 1));
            snprintf(r->channel, sizeof(r->channel), "%s", (const char *)sqlite3_column_text(stmt, 2));
            snprintf(r->nick, sizeof(r->nick), "%s", (const char *)sqlite3_column_text(stmt, 3));
            snprintf(r->role, sizeof(r->role), "%s", (const char *)sqlite3_column_text(stmt, 4));
            snprintf(r->content, sizeof(r->content), "%s", (const char *)sqlite3_column_text(stmt, 5));
            r->created_at = (time_t)sqlite3_column_int64(stmt, 6);
            count++;
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

int ai_orm_save_memory(AIORMHandle *handle, const AIMemoryRecord *record) {
    if (!handle || !record) return -1;

    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        const char *sql = "INSERT INTO ai_memories (profile_name, topic, summary, confidence, created_at) "
                          "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(handle->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            return -2;
        }

        sqlite3_bind_text(stmt, 1, record->profile_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, record->topic, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, record->summary, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, record->confidence);
        sqlite3_bind_int64(stmt, 5, record->created_at ? record->created_at : time(NULL));

        int step_rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (step_rc == SQLITE_DONE) ? 0 : -3;
    }
    return -1;
}

int ai_orm_query_memories(AIORMHandle *handle, const char *profile_name, const char *topic, int limit, AIMemoryRecord *out_records, int max_records) {
    if (!handle || !out_records || max_records <= 0) return 0;
    if (limit <= 0 || limit > max_records) limit = max_records;

    int count = 0;
    if (handle->backend == ORM_BACKEND_SQLITE && handle->sqlite_db) {
        sqlite3_stmt *stmt = NULL;
        const char *sql;
        if (topic && *topic) {
            sql = "SELECT id, profile_name, topic, summary, confidence, created_at "
                  "FROM ai_memories WHERE profile_name = ? AND topic LIKE ? "
                  "ORDER BY id DESC LIMIT ?;";
            if (sqlite3_prepare_v2(handle->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
                return 0;
            }
            char pattern[160];
            snprintf(pattern, sizeof(pattern), "%%%s%%", topic);
            sqlite3_bind_text(stmt, 1, profile_name ? profile_name : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, limit);
        } else {
            sql = "SELECT id, profile_name, topic, summary, confidence, created_at "
                  "FROM ai_memories WHERE profile_name = ? "
                  "ORDER BY id DESC LIMIT ?;";
            if (sqlite3_prepare_v2(handle->sqlite_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
                return 0;
            }
            sqlite3_bind_text(stmt, 1, profile_name ? profile_name : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, limit);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW && count < max_records) {
            AIMemoryRecord *r = &out_records[count];
            r->id = (long)sqlite3_column_int64(stmt, 0);
            snprintf(r->profile_name, sizeof(r->profile_name), "%s", (const char *)sqlite3_column_text(stmt, 1));
            snprintf(r->topic, sizeof(r->topic), "%s", (const char *)sqlite3_column_text(stmt, 2));
            snprintf(r->summary, sizeof(r->summary), "%s", (const char *)sqlite3_column_text(stmt, 3));
            r->confidence = sqlite3_column_double(stmt, 4);
            r->created_at = (time_t)sqlite3_column_int64(stmt, 5);
            count++;
        }
        sqlite3_finalize(stmt);
    }
    return count;
}
/* CPD-ON */
