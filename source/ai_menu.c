/*
 * Interactive ANSI Dialog Wizard & Menu for BitchX AI Subsystem
 */

#include "ai_menu.h"
#include "ai_profile.h"
#include "ai_honcho.h"
#include "ai_orm.h"
#include "ai_provider.h"

#ifdef HAVE_CONFIG_H
#include "irc.h"
#include "output.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

static void bx_ai_print(const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

#if defined(HAVE_CONFIG_H) && defined(BX_put_it)
    put_it("%s", buf);
#elif defined(HAVE_CONFIG_H)
    put_it("%s", buf);
#else
    printf("%s\n", buf);
#endif
}

#define BX_PRINT(fmt, ...) bx_ai_print(fmt, ##__VA_ARGS__)

/* ANSI Cyberpunk Color Codes */
#define C_RESET   "\033[0m"
#define C_CYAN    "\033[1;36m"
#define C_YELLOW  "\033[1;33m"
#define C_GREEN   "\033[1;32m"
#define C_MAGENTA "\033[1;35m"
#define C_RED     "\033[1;31m"
#define C_BLUE    "\033[1;34m"
#define C_WHITE   "\033[1;37m"
#define C_GRAY    "\033[0;37m"

static AIProfileManager g_ai_manager;
static int g_subsystem_initialized = 0;

AIProfileManager *ai_get_global_mgr(void) {
    if (!g_subsystem_initialized) {
        ai_init_global_subsystem();
    }
    return &g_ai_manager;
}

int ai_init_global_subsystem(void) {
    if (g_subsystem_initialized) return 0;
    ai_profile_mgr_init(&g_ai_manager);

    char config_path[512];
    ai_expand_path(AI_DEFAULT_CONFIG_PATH, config_path, sizeof(config_path));
    int rc = ai_profile_load_json(&g_ai_manager, config_path);

    /* If no config exists, seed with default OpenCode and Ollama presets */
    if (rc != 0 || g_ai_manager.count == 0) {
        AIProfile p1, p2;
        ai_profile_apply_preset(&p1, 0, "opencode");
        ai_profile_add_channel(&p1, "#revytech");
        ai_profile_add(&g_ai_manager, &p1);

        ai_profile_apply_preset(&p2, 2, "ollama");
        ai_profile_add(&g_ai_manager, &p2);

        ai_profile_set_active(&g_ai_manager, "opencode");
        ai_profile_save_json(&g_ai_manager, config_path);
    }

    g_subsystem_initialized = 1;
    return 0;
}

int ai_save_global_mgr(void) {
    char config_path[512];
    ai_expand_path(AI_DEFAULT_CONFIG_PATH, config_path, sizeof(config_path));
    return ai_profile_save_json(&g_ai_manager, config_path);
}

void ai_menu_list_presets(void) {
    BX_PRINT("%s╔══════════════════════════════════════════════════════════════════════════════╗%s", C_CYAN, C_RESET);
    BX_PRINT("%s║   %s★ BITCHX AI PROVIDER PRESET CATALOG (Quick-Select) ★%s                   ║%s", C_CYAN, C_YELLOW, C_CYAN, C_RESET);
    BX_PRINT("%s╠═════╦══════════════════╦═════════════════════╦═════════════════════════════╣%s", C_CYAN, C_RESET);
    BX_PRINT("%s║ %sID%s  ║ %sPRESET%s           ║ %sCHAT MODEL%s        ║ %sMEMORY STORE%s              ║%s",
             C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_RESET);
    BX_PRINT("%s╠═════╬══════════════════╬═════════════════════╬═════════════════════════════╣%s", C_CYAN, C_RESET);

    int count = ai_profile_get_presets_count();
    for (int i = 0; i < count; i++) {
        const AIProfilePreset *p = ai_profile_get_preset(i);
        const char *mem_type = (p->memory_type == AI_MEMORY_EXTERNAL_HONCHO) ?
                               "Honcho Dialectic" : "Internal SQLite";
        BX_PRINT("%s║ %s[%d]%s ║ %-16.16s ║ %-19.19s ║ %-27.27s ║%s",
                 C_CYAN, C_YELLOW, i + 1, C_CYAN,
                 p->preset_name, p->chat_model, mem_type, C_RESET);
        BX_PRINT("%s║     ║   %s↳ %-66.66s%s ║%s",
                 C_CYAN, C_GRAY, p->description, C_CYAN, C_RESET);
    }
    BX_PRINT("%s╚═════╩══════════════════╩═════════════════════╩═════════════════════════════╝%s", C_CYAN, C_RESET);
    BX_PRINT("%sUse %s/AIPROFILE CREATE <name> <preset_id_or_name>%s to create without retyping endpoints.%s",
             C_WHITE, C_YELLOW, C_WHITE, C_RESET);
}

void ai_menu_list_profiles(const AIProfileManager *mgr) {
    if (!mgr) mgr = ai_get_global_mgr();

    BX_PRINT("%s╔══════════════════════════════════════════════════════════════════════════════╗%s", C_CYAN, C_RESET);
    BX_PRINT("%s║   %s★ BITCHX AI CONFIGURATION PROFILES ★%s                                   ║%s", C_CYAN, C_YELLOW, C_CYAN, C_RESET);
    BX_PRINT("%s╠════╦══════════════════╦═════════════════════╦════════════════╦═════════════╣%s", C_CYAN, C_RESET);
    BX_PRINT("%s║ %sST%s ║ %sNAME%s             ║ %sCHAT MODEL%s        ║ %sMEMORY%s         ║ %sCHANNELS%s    ║%s",
             C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_WHITE, C_CYAN, C_RESET);
    BX_PRINT("%s╠════╬══════════════════╬═════════════════════╬════════════════╬═════════════╣%s", C_CYAN, C_RESET);

    for (int i = 0; i < mgr->count; i++) {
        const AIProfile *p = &mgr->profiles[i];
        int is_act = !strcmp(mgr->active_profile, p->name);
        const char *st_tag = is_act ? " ★" : "  ";
        const char *st_col = is_act ? C_GREEN : C_GRAY;
        const char *mem_str = (p->memory_type == AI_MEMORY_EXTERNAL_HONCHO) ? "Honcho" : "SQLite";

        char chans[32] = "";
        if (p->num_channels == 0) {
            snprintf(chans, sizeof(chans), "(all/default)");
        } else {
            snprintf(chans, sizeof(chans), "%d chan(s)", p->num_channels);
        }

        BX_PRINT("%s║ %s%s%s ║ %s%-16.16s%s ║ %-19.19s ║ %-14.14s ║ %-11.11s ║%s",
                 C_CYAN, st_col, st_tag, C_CYAN,
                 is_act ? C_YELLOW : C_WHITE, p->name, C_CYAN,
                 p->chat_model, mem_str, chans, C_RESET);
    }

    if (mgr->count == 0) {
        BX_PRINT("%s║   %s(No profiles configured. Use /AIPROFILE CREATE to add one)%s             ║%s",
                 C_CYAN, C_RED, C_CYAN, C_RESET);
    }

    BX_PRINT("%s╚════╩══════════════════╩═════════════════════╩════════════════╩═════════════╝%s", C_CYAN, C_RESET);
}

void ai_menu_render_profile_card(const AIProfile *p) {
    if (!p) return;

    BX_PRINT("%s╔══════════════════════════════════════════════════════════════════════════════╗%s", C_CYAN, C_RESET);
    BX_PRINT("%s║ %sPROFILE: %-20.20s%s                 %sSTATUS: %-15.15s%s ║%s",
             C_CYAN, C_YELLOW, p->name, C_CYAN,
             p->is_active ? C_GREEN : C_GRAY,
             p->is_active ? "[ACTIVE]" : "[INACTIVE]", C_CYAN, C_RESET);
    BX_PRINT("%s╠══════════════════════════════════════════════════════════════════════════════╣%s", C_CYAN, C_RESET);
    BX_PRINT("%s║ %sChat Model     :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_YELLOW, p->chat_model, C_CYAN, C_RESET);
    BX_PRINT("%s║ %sAPI Endpoint   :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_WHITE, p->api_endpoint, C_CYAN, C_RESET);
    BX_PRINT("%s║ %sKey Env Var    :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_WHITE, p->api_key_env[0] ? p->api_key_env : "(none/direct)", C_CYAN, C_RESET);
    BX_PRINT("%s║ %sTrigger Flags  :%s %s%s%s%s %-39.39s %s║%s",
             C_CYAN, C_WHITE, C_GREEN,
             (p->trigger_mode & AI_TRIGGER_COMMAND) ? "[CMD]" : "",
             (p->trigger_mode & AI_TRIGGER_MENTION) ? "[MENTION]" : "",
             (p->trigger_mode & AI_TRIGGER_QUERY) ? "[QUERY]" : "",
             (p->trigger_mode & AI_TRIGGER_AUTONOMOUS) ? "[AUTONOMOUS-REFLEX]" : "",
             "", C_CYAN, C_RESET);

    if (p->memory_type == AI_MEMORY_EXTERNAL_HONCHO) {
        BX_PRINT("%s╠┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄╣%s", C_CYAN, C_RESET);
        BX_PRINT("%s║ %sMemory Backend :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_MAGENTA, "External Honcho Memory", C_CYAN, C_RESET);
        BX_PRINT("%s║ %sHoncho Workspace:%s %-53.53s %s║%s", C_CYAN, C_WHITE, C_WHITE, p->honcho_config.workspace, C_CYAN, C_RESET);
        BX_PRINT("%s║ %sHoncho Endpoint:%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_WHITE, p->honcho_config.endpoint, C_CYAN, C_RESET);
        BX_PRINT("%s║ %sAuto Mappings  :%s Channel->Session: %s, Nick->Peer: %s %-18.18s %s║%s",
                 C_CYAN, C_WHITE, C_GREEN,
                 p->honcho_config.auto_map_channel_session ? "YES" : "NO",
                 p->honcho_config.auto_map_nick_peer ? "YES" : "NO",
                 "", C_CYAN, C_RESET);
    } else {
        BX_PRINT("%s╠┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄╣%s", C_CYAN, C_RESET);
        BX_PRINT("%s║ %sMemory Backend :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_MAGENTA, "Internal SQLite3 Database ORM", C_CYAN, C_RESET);
        BX_PRINT("%s║ %sDatabase Path  :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_WHITE, p->internal_config.db_path, C_CYAN, C_RESET);
        BX_PRINT("%s║ %sSystem 1 Model :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_YELLOW,
                 p->internal_config.system1_model[0] ? p->internal_config.system1_model : "(none)", C_CYAN, C_RESET);
    }

    BX_PRINT("%s╠┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄╣%s", C_CYAN, C_RESET);
    char ch_buf[256] = "";
    for (int i = 0; i < p->num_channels; i++) {
        strncat(ch_buf, p->channels[i], sizeof(ch_buf) - strlen(ch_buf) - 2);
        if (i < p->num_channels - 1) strncat(ch_buf, ", ", sizeof(ch_buf) - strlen(ch_buf) - 1);
    }
    BX_PRINT("%s║ %sBound Channels :%s %-54.54s %s║%s", C_CYAN, C_WHITE, C_WHITE, ch_buf[0] ? ch_buf : "(none - active profile fallback)", C_CYAN, C_RESET);
    BX_PRINT("%s╚══════════════════════════════════════════════════════════════════════════════╝%s", C_CYAN, C_RESET);
}

/* #lizard forgives -- CLI command router for profile subcommands */
void aiprofile_command(const char *cmd, char *args) {
    (void)cmd;
    AIProfileManager *mgr = ai_get_global_mgr();

    if (!args || !*args) {
        ai_menu_list_profiles(mgr);
        BX_PRINT("%sUsage: /AIPROFILE [LIST | PRESETS | SHOW <name> | USE <name> | CREATE <name> [preset] | DELETE <name> | BIND <chan> [name] | UNBIND <chan> [name] | BACKUP [file] | RESTORE <file>]%s",
                 C_YELLOW, C_RESET);
        return;
    }

    char subcmd[64] = "";
    char arg1[128] = "";
    char arg2[128] = "";
    sscanf(args, "%63s %127s %127s", subcmd, arg1, arg2);

    if (!strcasecmp(subcmd, "LIST")) {
        ai_menu_list_profiles(mgr);
    } else if (!strcasecmp(subcmd, "PRESETS")) {
        ai_menu_list_presets();
    } else if (!strcasecmp(subcmd, "SHOW")) {
        const char *target = arg1[0] ? arg1 : mgr->active_profile;
        AIProfile *p = ai_profile_get(mgr, target);
        if (p) {
            ai_menu_render_profile_card(p);
        } else {
            BX_PRINT("%s[AI] Profile '%s' not found.%s", C_RED, target, C_RESET);
        }
    } else if (!strcasecmp(subcmd, "USE")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE USE <profile_name>%s", C_RED, C_RESET);
            return;
        }
        if (ai_profile_set_active(mgr, arg1) == 0) {
            ai_save_global_mgr();
            BX_PRINT("%s[AI] Active AI Profile switched to: %s%s%s", C_GREEN, C_YELLOW, arg1, C_RESET);
        } else {
            BX_PRINT("%s[AI] Profile '%s' does not exist.%s", C_RED, arg1, C_RESET);
        }
    } else if (!strcasecmp(subcmd, "CREATE")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE CREATE <name> [preset_number_or_name]%s", C_RED, C_RESET);
            return;
        }

        AIProfile new_prof;
        int preset_idx = 0;
        if (arg2[0]) {
            if (isdigit((unsigned char)arg2[0])) {
                preset_idx = atoi(arg2) - 1;
                if (preset_idx < 0 || preset_idx >= ai_profile_get_presets_count()) {
                    preset_idx = 0;
                }
            } else {
                for (int i = 0; i < ai_profile_get_presets_count(); i++) {
                    if (!strcasecmp(ai_profile_get_preset(i)->preset_name, arg2)) {
                        preset_idx = i;
                        break;
                    }
                }
            }
        }

        ai_profile_apply_preset(&new_prof, preset_idx, arg1);
        int rc = ai_profile_add(mgr, &new_prof);
        if (rc == 0) {
            ai_save_global_mgr();
            BX_PRINT("%s[AI] Profile '%s' created from preset '%s'!%s",
                     C_GREEN, arg1, ai_profile_get_preset(preset_idx)->preset_name, C_RESET);
            ai_menu_render_profile_card(&new_prof);
        } else if (rc == -3) {
            BX_PRINT("%s[AI] Profile '%s' already exists.%s", C_RED, arg1, C_RESET);
        } else {
            BX_PRINT("%s[AI] Failed to create profile (error %d).%s", C_RED, rc, C_RESET);
        }
    } else if (!strcasecmp(subcmd, "DELETE")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE DELETE <name>%s", C_RED, C_RESET);
            return;
        }
        if (ai_profile_delete(mgr, arg1) == 0) {
            ai_save_global_mgr();
            BX_PRINT("%s[AI] Profile '%s' deleted.%s", C_GREEN, arg1, C_RESET);
        } else {
            BX_PRINT("%s[AI] Profile '%s' not found.%s", C_RED, arg1, C_RESET);
        }
    } else if (!strcasecmp(subcmd, "BIND")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE BIND <channel> [profile_name]%s", C_RED, C_RESET);
            return;
        }
        const char *pname = arg2[0] ? arg2 : mgr->active_profile;
        AIProfile *p = ai_profile_get(mgr, pname);
        if (!p) {
            BX_PRINT("%s[AI] Profile '%s' not found.%s", C_RED, pname, C_RESET);
            return;
        }
        ai_profile_add_channel(p, arg1);
        ai_save_global_mgr();
        BX_PRINT("%s[AI] Bound channel %s%s%s to profile %s%s%s.%s",
                 C_GREEN, C_YELLOW, arg1, C_GREEN, C_YELLOW, p->name, C_GREEN, C_RESET);
    } else if (!strcasecmp(subcmd, "UNBIND")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE UNBIND <channel> [profile_name]%s", C_RED, C_RESET);
            return;
        }
        const char *pname = arg2[0] ? arg2 : mgr->active_profile;
        AIProfile *p = ai_profile_get(mgr, pname);
        if (!p) {
            BX_PRINT("%s[AI] Profile '%s' not found.%s", C_RED, pname, C_RESET);
            return;
        }
        ai_profile_remove_channel(p, arg1);
        ai_save_global_mgr();
        BX_PRINT("%s[AI] Unbound channel %s%s%s from profile %s%s%s.%s",
                 C_GREEN, C_YELLOW, arg1, C_GREEN, C_YELLOW, p->name, C_GREEN, C_RESET);
    } else if (!strcasecmp(subcmd, "BACKUP")) {
        const char *bpath = arg1[0] ? arg1 : NULL;
        int rc = ai_profile_backup(mgr, bpath);
        if (rc == 0) {
            BX_PRINT("%s[AI] AI Profiles successfully backed up.%s", C_GREEN, C_RESET);
        } else {
            BX_PRINT("%s[AI] AI Profile backup failed (error %d).%s", C_RED, rc, C_RESET);
        }
    } else if (!strcasecmp(subcmd, "RESTORE")) {
        if (!arg1[0]) {
            BX_PRINT("%s[AI] Usage: /AIPROFILE RESTORE <file_path>%s", C_RED, C_RESET);
            return;
        }
        int rc = ai_profile_restore(mgr, arg1);
        if (rc == 0) {
            ai_save_global_mgr();
            BX_PRINT("%s[AI] AI Profiles successfully restored from '%s'!%s", C_GREEN, arg1, C_RESET);
            ai_menu_list_profiles(mgr);
        } else {
            BX_PRINT("%s[AI] Failed to restore profiles from '%s' (error %d).%s", C_RED, arg1, rc, C_RESET);
        }
    } else {
        BX_PRINT("%s[AI] Unknown subcommand '%s'. Type /AIPROFILE for usage.%s", C_RED, subcmd, C_RESET);
    }
}

void savesession_command(const char *cmd, char *args) {
    (void)cmd;
    AISessionState sess;
    ai_session_state_init(&sess);

    AIProfileManager *mgr = ai_get_global_mgr();
    if (mgr->active_profile[0]) {
        snprintf(sess.active_ai_profile, sizeof(sess.active_ai_profile), "%s", mgr->active_profile);
    }

    const char *path = (args && *args) ? args : AI_DEFAULT_SESSION_PATH;
    int rc = ai_session_save(&sess, path);
    if (rc == 0) {
        char exp_path[512];
        ai_expand_path(path, exp_path, sizeof(exp_path));
        BX_PRINT("%s[SESSION] Session state saved to %s%s%s", C_GREEN, C_YELLOW, exp_path, C_RESET);
    } else {
        BX_PRINT("%s[SESSION] Failed to save session state (code %d).%s", C_RED, rc, C_RESET);
    }
}

void restoresession_command(const char *cmd, char *args) {
    (void)cmd;
    AISessionState sess;
    const char *path = (args && *args) ? args : AI_DEFAULT_SESSION_PATH;
    int rc = ai_session_load(&sess, path);
    if (rc == 0) {
        char exp_path[512];
        ai_expand_path(path, exp_path, sizeof(exp_path));
        BX_PRINT("%s[SESSION] Session restored from %s%s%s", C_GREEN, C_YELLOW, exp_path, C_RESET);
        if (sess.active_ai_profile[0]) {
            AIProfileManager *mgr = ai_get_global_mgr();
            ai_profile_set_active(mgr, sess.active_ai_profile);
            BX_PRINT("%s[SESSION] Active AI Profile restored to: %s%s%s",
                     C_GREEN, C_YELLOW, sess.active_ai_profile, C_RESET);
        }
    } else {
        BX_PRINT("%s[SESSION] Failed to restore session from '%s' (code %d).%s", C_RED, path, rc, C_RESET);
    }
}
