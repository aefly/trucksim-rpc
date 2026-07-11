#pragma once
#include "scssdk.h"

void log_init(scs_log_t log_fn);

void log_init_start();
void log_init_discord_fail();
void log_init_discord_ok();
void log_game_info(const char *game_name, scs_u32_t version);
void log_discord_reconnect();
void log_discord_shutdown();
void log_warn(const char *fmt, ...);
void log_init_complete();
