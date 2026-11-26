/*
 * Unit Tests for Universal AI Provider Client (CMocka)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai_provider.h"

static void test_provider_url_resolution(void **state) {
    (void)state;
    char url[256];

    ai_provider_resolve_url("http://localhost:4000/v1", url, sizeof(url));
    assert_string_equal(url, "http://localhost:4000/v1/chat/completions");

    ai_provider_resolve_url("http://localhost:4000/v1/", url, sizeof(url));
    assert_string_equal(url, "http://localhost:4000/v1/chat/completions");

    ai_provider_resolve_url("https://api.openai.com/v1/chat/completions", url, sizeof(url));
    assert_string_equal(url, "https://api.openai.com/v1/chat/completions");
}

static void test_provider_system1_evaluation_logic(void **state) {
    (void)state;
    AIProfile prof;
    memset(&prof, 0, sizeof(prof));

    /* When autonomous trigger flag is disabled, it should return 0 */
    prof.trigger_mode = AI_TRIGGER_COMMAND;
    char reason[128];
    int res = ai_provider_evaluate_system1(&prof, "#chan", "nick", "help me with code", reason, sizeof(reason));
    assert_int_equal(res, 0);

    /* Negative: NULL profile or empty message */
    assert_int_equal(ai_provider_evaluate_system1(NULL, "#chan", "nick", "hello", reason, sizeof(reason)), 0);
    assert_int_equal(ai_provider_evaluate_system1(&prof, "#chan", "nick", NULL, reason, sizeof(reason)), 0);
}

static void test_provider_init_cleanup(void **state) {
    (void)state;
    assert_int_equal(ai_provider_init(), 0);
    ai_provider_cleanup();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_provider_url_resolution),
        cmocka_unit_test(test_provider_system1_evaluation_logic),
        cmocka_unit_test(test_provider_init_cleanup),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
