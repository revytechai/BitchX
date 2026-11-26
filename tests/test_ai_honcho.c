/*
 * Unit Tests for Honcho HTTP Client and ID Resolver (CMocka)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai_honcho.h"

static void test_honcho_resolve_ids_auto(void **state) {
    (void)state;
    AIHonchoConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.workspace, sizeof(cfg.workspace), "revytech");
    cfg.auto_map_channel_session = 1;
    cfg.auto_map_nick_peer = 1;

    char session[128] = "";
    char peer[128] = "";

    ai_honcho_resolve_ids(&cfg, "#revytech-dev", "CyberPunk_99",
                          session, sizeof(session),
                          peer, sizeof(peer));

    assert_string_equal(session, "revytech-dev");
    assert_string_equal(peer, "cyberpunk_99");
}

static void test_honcho_resolve_ids_fallback(void **state) {
    (void)state;
    AIHonchoConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.workspace, sizeof(cfg.workspace), "revytech");
    snprintf(cfg.session_id, sizeof(cfg.session_id), "default_session");
    snprintf(cfg.peer_id, sizeof(cfg.peer_id), "default_peer");
    cfg.auto_map_channel_session = 0;
    cfg.auto_map_nick_peer = 0;

    char session[128] = "";
    char peer[128] = "";

    ai_honcho_resolve_ids(&cfg, "#somechan", "somenick",
                          session, sizeof(session),
                          peer, sizeof(peer));

    assert_string_equal(session, "default_session");
    assert_string_equal(peer, "default_peer");
}

static void test_honcho_global_init(void **state) {
    (void)state;
    assert_int_equal(ai_honcho_global_init(), 0);
    ai_honcho_global_cleanup();
}

static void test_honcho_null_negative(void **state) {
    (void)state;
    AIHonchoResponse resp;
    assert_int_equal(ai_honcho_get_peer_context(NULL, "peer", &resp), -1);
    assert_int_equal(ai_honcho_get_session_context(NULL, "session", &resp), -1);
    assert_int_equal(ai_honcho_create_conclusion(NULL, "s", "p", "c", &resp), -1);
    assert_int_equal(ai_honcho_ping(NULL, &resp), -1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_honcho_resolve_ids_auto),
        cmocka_unit_test(test_honcho_resolve_ids_fallback),
        cmocka_unit_test(test_honcho_global_init),
        cmocka_unit_test(test_honcho_null_negative),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
