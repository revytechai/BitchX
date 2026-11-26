/*
 * Unit Tests for AI Profile and Session Persistence (CMocka)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ai_profile.h"

static void test_profile_init_and_empty(void **state) {
    (void)state;
    AIProfileManager mgr;
    ai_profile_mgr_init(&mgr);

    assert_int_equal(mgr.count, 0);
    assert_string_equal(mgr.active_profile, "");
    assert_null(ai_profile_get_active(&mgr));
    assert_null(ai_profile_get(&mgr, "nonexistent"));
}

static void test_profile_add_and_get(void **state) {
    (void)state;
    AIProfileManager mgr;
    ai_profile_mgr_init(&mgr);

    AIProfile p1;
    memset(&p1, 0, sizeof(p1));
    snprintf(p1.name, sizeof(p1.name), "test_opencode");
    snprintf(p1.chat_model, sizeof(p1.chat_model), "gpt-4o");
    snprintf(p1.api_endpoint, sizeof(p1.api_endpoint), "http://localhost:4000/v1");
    p1.memory_type = AI_MEMORY_EXTERNAL_HONCHO;

    int rc = ai_profile_add(&mgr, &p1);
    assert_int_equal(rc, 0);
    assert_int_equal(mgr.count, 1);
    assert_string_equal(mgr.active_profile, "test_opencode");

    AIProfile *ret = ai_profile_get(&mgr, "test_opencode");
    assert_non_null(ret);
    assert_string_equal(ret->chat_model, "gpt-4o");
    assert_int_equal(ret->memory_type, AI_MEMORY_EXTERNAL_HONCHO);

    /* Negative: duplicate name */
    int dup_rc = ai_profile_add(&mgr, &p1);
    assert_int_equal(dup_rc, -3);

    /* Negative: null or empty name */
    assert_int_equal(ai_profile_add(&mgr, NULL), -1);
    AIProfile empty_p;
    memset(&empty_p, 0, sizeof(empty_p));
    assert_int_equal(ai_profile_add(&mgr, &empty_p), -1);
}

static void test_preset_catalog(void **state) {
    (void)state;
    int count = ai_profile_get_presets_count();
    assert_true(count >= 5);

    const AIProfilePreset *p0 = ai_profile_get_preset(0);
    assert_non_null(p0);
    assert_string_equal(p0->preset_name, "opencode");

    AIProfile prof;
    int rc = ai_profile_apply_preset(&prof, 0, "my_opencode");
    assert_int_equal(rc, 0);
    assert_string_equal(prof.name, "my_opencode");
    assert_string_equal(prof.chat_model, "gpt-4o");
    assert_int_equal(prof.memory_type, AI_MEMORY_EXTERNAL_HONCHO);
    assert_int_equal(prof.honcho_config.auto_map_channel_session, 1);

    /* Negative: invalid preset index */
    assert_int_equal(ai_profile_apply_preset(&prof, -1, "bad"), -1);
    assert_int_equal(ai_profile_apply_preset(&prof, 999, "bad"), -1);
}

static void test_profile_channel_binding(void **state) {
    (void)state;
    AIProfile prof;
    memset(&prof, 0, sizeof(prof));
    snprintf(prof.name, sizeof(prof.name), "chan_test");

    assert_int_equal(ai_profile_add_channel(&prof, "#revytech"), 0);
    assert_int_equal(ai_profile_add_channel(&prof, "#bitchx"), 0);
    assert_int_equal(prof.num_channels, 2);

    /* Duplicate channel should not duplicate entry */
    assert_int_equal(ai_profile_add_channel(&prof, "#revytech"), 0);
    assert_int_equal(prof.num_channels, 2);

    /* Remove channel */
    assert_int_equal(ai_profile_remove_channel(&prof, "#revytech"), 0);
    assert_int_equal(prof.num_channels, 1);
    assert_string_equal(prof.channels[0], "#bitchx");

    /* Remove non-existent channel */
    assert_int_equal(ai_profile_remove_channel(&prof, "#nowhere"), -2);
}

static void test_profile_save_load_backup_restore(void **state) {
    (void)state;
    const char *tmp_json = "/tmp/bx_test_profiles.json";
    const char *tmp_bak = "/tmp/bx_test_backup.json";
    unlink(tmp_json);
    unlink(tmp_bak);

    AIProfileManager mgr;
    ai_profile_mgr_init(&mgr);

    AIProfile p1;
    ai_profile_apply_preset(&p1, 0, "opencode_prod");
    ai_profile_add_channel(&p1, "#security");
    ai_profile_add(&mgr, &p1);

    AIProfile p2;
    ai_profile_apply_preset(&p2, 2, "ollama_local");
    ai_profile_add_channel(&p2, "#internal");
    ai_profile_add(&mgr, &p2);

    ai_profile_set_active(&mgr, "ollama_local");

    /* Save to JSON */
    int save_rc = ai_profile_save_json(&mgr, tmp_json);
    assert_int_equal(save_rc, 0);

    /* Backup */
    int bak_rc = ai_profile_backup(&mgr, tmp_bak);
    assert_int_equal(bak_rc, 0);

    /* Load into a new manager */
    AIProfileManager loaded_mgr;
    int load_rc = ai_profile_load_json(&loaded_mgr, tmp_json);
    assert_int_equal(load_rc, 0);
    assert_int_equal(loaded_mgr.count, 2);
    assert_string_equal(loaded_mgr.active_profile, "ollama_local");

    AIProfile *p_loaded = ai_profile_get(&loaded_mgr, "opencode_prod");
    assert_non_null(p_loaded);
    assert_int_equal(p_loaded->memory_type, AI_MEMORY_EXTERNAL_HONCHO);
    assert_int_equal(p_loaded->num_channels, 1);
    assert_string_equal(p_loaded->channels[0], "#security");

    /* Restore from backup */
    AIProfileManager restored_mgr;
    int res_rc = ai_profile_restore(&restored_mgr, tmp_bak);
    assert_int_equal(res_rc, 0);
    assert_int_equal(restored_mgr.count, 2);

    unlink(tmp_json);
    unlink(tmp_bak);
}

static void test_session_state_save_load(void **state) {
    (void)state;
    const char *tmp_session = "/tmp/bx_test_session.json";
    unlink(tmp_session);

    AISessionState sess;
    ai_session_state_init(&sess);
    snprintf(sess.nickname, sizeof(sess.nickname), "cyberpunk");
    snprintf(sess.realname, sizeof(sess.realname), "BitchX User");
    snprintf(sess.default_server, sizeof(sess.default_server), "irc.cloudbsd.org");
    sess.default_port = 6697;
    sess.use_ssl = 1;
    snprintf(sess.active_ai_profile, sizeof(sess.active_ai_profile), "opencode_prod");
    snprintf(sess.joined_channels[0], sizeof(sess.joined_channels[0]), "#revytech");
    snprintf(sess.joined_channels[1], sizeof(sess.joined_channels[1]), "#bitchx");
    sess.num_joined_channels = 2;

    int rc = ai_session_save(&sess, tmp_session);
    assert_int_equal(rc, 0);

    AISessionState loaded;
    rc = ai_session_load(&loaded, tmp_session);
    assert_int_equal(rc, 0);
    assert_string_equal(loaded.nickname, "cyberpunk");
    assert_string_equal(loaded.default_server, "irc.cloudbsd.org");
    assert_int_equal(loaded.default_port, 6697);
    assert_int_equal(loaded.use_ssl, 1);
    assert_string_equal(loaded.active_ai_profile, "opencode_prod");
    assert_int_equal(loaded.num_joined_channels, 2);
    assert_string_equal(loaded.joined_channels[0], "#revytech");
    assert_string_equal(loaded.joined_channels[1], "#bitchx");

    unlink(tmp_session);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_profile_init_and_empty),
        cmocka_unit_test(test_profile_add_and_get),
        cmocka_unit_test(test_preset_catalog),
        cmocka_unit_test(test_profile_channel_binding),
        cmocka_unit_test(test_profile_save_load_backup_restore),
        cmocka_unit_test(test_session_state_save_load),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
