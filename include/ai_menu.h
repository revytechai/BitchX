/*
 * Interactive ANSI Dialog Wizard & Menu for BitchX AI Subsystem
 * Cyberpunk look & feel with preset auto-selection and zero external dependencies.
 */

#ifndef _AI_MENU_H_
#define _AI_MENU_H_

#include "ai_profile.h"

/* Render formatted cyberpunk list of profiles to BitchX output */
void ai_menu_list_profiles(const AIProfileManager *mgr);

/* Render preset catalog */
void ai_menu_list_presets(void);

/* Render interactive ANSI dialog box displaying profile configuration */
void ai_menu_render_profile_card(const AIProfile *p);

/* CLI command handler for /AIPROFILE */
void aiprofile_command(const char *cmd, char *args);

/* CLI command handler for /SAVESESSION and /RESTORESESSION */
void savesession_command(const char *cmd, char *args);
void restoresession_command(const char *cmd, char *args);

/* Global manager accessors */
AIProfileManager *ai_get_global_mgr(void);
int               ai_save_global_mgr(void);
int               ai_init_global_subsystem(void);

#endif /* _AI_MENU_H_ */
