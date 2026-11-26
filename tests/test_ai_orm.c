/*
 * Unit Tests for Database ORM Abstraction Layer (SQLite3) (CMocka)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ai_orm.h"

static const char *TEST_DB_PATH = "/tmp/test_ai_memory.db";

static void test_orm_connect_disconnect(void **state) {
    (void)state;
    unlink(TEST_DB_PATH);

    AIORMHandle *handle = ai_orm_connect(ORM_BACKEND_SQLITE, TEST_DB_PATH);
    assert_non_null(handle);
    assert_int_equal(ai_orm_get_backend(handle), ORM_BACKEND_SQLITE);

    int rc = ai_orm_disconnect(handle);
    assert_int_equal(rc, 0);

    /* Negative test: NULL connection string */
    assert_null(ai_orm_connect(ORM_BACKEND_SQLITE, NULL));
    assert_int_equal(ai_orm_disconnect(NULL), -1);

    unlink(TEST_DB_PATH);
}

/* CPD-OFF -- ORM test fixtures */
static void test_orm_message_crud(void **state) {
    (void)state;
    unlink(TEST_DB_PATH);

    AIORMHandle *handle = ai_orm_connect(ORM_BACKEND_SQLITE, TEST_DB_PATH);
    assert_non_null(handle);

    AIMessageRecord msg1;
    memset(&msg1, 0, sizeof(msg1));
    snprintf(msg1.profile_name, sizeof(msg1.profile_name), "opencode");
    snprintf(msg1.channel, sizeof(msg1.channel), "#revytech");
    snprintf(msg1.nick, sizeof(msg1.nick), "alice");
    snprintf(msg1.role, sizeof(msg1.role), "user");
    snprintf(msg1.content, sizeof(msg1.content), "Hello BitchX AI!");

    int save_rc = ai_orm_save_message(handle, &msg1);
    assert_int_equal(save_rc, 0);

    AIMessageRecord msg2;
    memset(&msg2, 0, sizeof(msg2));
    snprintf(msg2.profile_name, sizeof(msg2.profile_name), "opencode");
    snprintf(msg2.channel, sizeof(msg2.channel), "#revytech");
    snprintf(msg2.nick, sizeof(msg2.nick), "bitchx_bot");
    snprintf(msg2.role, sizeof(msg2.role), "assistant");
    snprintf(msg2.content, sizeof(msg2.content), "Greetings, Alice.");

    save_rc = ai_orm_save_message(handle, &msg2);
    assert_int_equal(save_rc, 0);

    /* Query messages back (most recent first due to ORDER BY id DESC) */
    AIMessageRecord fetched[10];
    int count = ai_orm_get_messages(handle, "opencode", "#revytech", 10, fetched, 10);
    assert_int_equal(count, 2);
    assert_string_equal(fetched[0].nick, "bitchx_bot");
    assert_string_equal(fetched[0].content, "Greetings, Alice.");
    assert_string_equal(fetched[1].nick, "alice");
    assert_string_equal(fetched[1].content, "Hello BitchX AI!");

    /* Query non-existent channel */
    count = ai_orm_get_messages(handle, "opencode", "#other", 10, fetched, 10);
    assert_int_equal(count, 0);

    /* Negative: invalid arguments */
    assert_int_equal(ai_orm_save_message(NULL, &msg1), -1);
    assert_int_equal(ai_orm_save_message(handle, NULL), -1);
    assert_int_equal(ai_orm_get_messages(NULL, "opencode", "#revytech", 10, fetched, 10), 0);

    ai_orm_disconnect(handle);
    unlink(TEST_DB_PATH);
}

static void test_orm_memory_crud(void **state) {
    (void)state;
    unlink(TEST_DB_PATH);

    AIORMHandle *handle = ai_orm_connect(ORM_BACKEND_SQLITE, TEST_DB_PATH);
    assert_non_null(handle);

    AIMemoryRecord mem1;
    memset(&mem1, 0, sizeof(mem1));
    snprintf(mem1.profile_name, sizeof(mem1.profile_name), "opencode");
    snprintf(mem1.topic, sizeof(mem1.topic), "deployment");
    snprintf(mem1.summary, sizeof(mem1.summary), "Production server runs on FreeBSD 16 ARM64.");
    mem1.confidence = 0.95;

    int rc = ai_orm_save_memory(handle, &mem1);
    assert_int_equal(rc, 0);

    AIMemoryRecord mem2;
    memset(&mem2, 0, sizeof(mem2));
    snprintf(mem2.profile_name, sizeof(mem2.profile_name), "opencode");
    snprintf(mem2.topic, sizeof(mem2.topic), "toolchain");
    snprintf(mem2.summary, sizeof(mem2.summary), "Using Clang C17 compiler.");
    mem2.confidence = 0.88;

    rc = ai_orm_save_memory(handle, &mem2);
    assert_int_equal(rc, 0);

    /* Query all memories for profile */
    AIMemoryRecord results[5];
    int count = ai_orm_query_memories(handle, "opencode", NULL, 5, results, 5);
    assert_int_equal(count, 2);

    /* Query specific topic filter */
    count = ai_orm_query_memories(handle, "opencode", "deployment", 5, results, 5);
    assert_int_equal(count, 1);
    assert_string_equal(results[0].topic, "deployment");
    assert_string_equal(results[0].summary, "Production server runs on FreeBSD 16 ARM64.");

    /* Negative: query non-existent topic */
    count = ai_orm_query_memories(handle, "opencode", "nonexistent_topic", 5, results, 5);
    assert_int_equal(count, 0);

    ai_orm_disconnect(handle);
    unlink(TEST_DB_PATH);
}
/* CPD-ON */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_orm_connect_disconnect),
        cmocka_unit_test(test_orm_message_crud),
        cmocka_unit_test(test_orm_memory_crud),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
