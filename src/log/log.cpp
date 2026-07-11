#include "log.h"
#include <cstdarg>
#include <cstdio>

static scs_log_t g_fn = nullptr;

void log_init(scs_log_t log_fn) { g_fn = log_fn; }

static void vlog(scs_log_type_t type, const char *fmt, va_list args) {
#ifndef NDEBUG
  if (!g_fn)
    return;
  char buf[512];
  int n = snprintf(buf, sizeof(buf), "[TruckSimRPC] ");
  vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
  g_fn(type, buf);
#else
  (void)type;
  (void)fmt;
  (void)args;
#endif
}

static void msg(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vlog(SCS_LOG_TYPE_message, fmt, args);
  va_end(args);
}

void log_warn(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vlog(SCS_LOG_TYPE_warning, fmt, args);
  va_end(args);
}

static void err(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vlog(SCS_LOG_TYPE_error, fmt, args);
  va_end(args);
}

void log_game_info(const char *game_name, scs_u32_t version) {
  msg("Game: %s v%u.%u", game_name, SCS_GET_MAJOR_VERSION(version),
      SCS_GET_MINOR_VERSION(version));
}

void log_init_start() { msg("Initializing TruckSim RPC..."); }

void log_init_discord_fail() { err("Discord Game SDK initialization failed"); }
void log_init_discord_ok() { msg("Discord Game SDK initialized"); }
void log_discord_reconnect() { msg("Discord reconnected"); }
void log_discord_shutdown() { msg("Discord shutdown"); }
void log_init_complete() { msg("TruckSim RPC is running!"); }
