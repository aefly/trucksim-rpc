#include "presence.h"
#include "config.h"
#include "discord_client.h"
#include "log.h"
#include "telemetry.h"
#include <cstdio>
#include <cstring>
#include <ctime>

static DiscordClient s_discord;
static bool s_discord_inited = false;

// Cached values to avoid redundant Discord SDK calls
static char s_last_details[128] = {};
static char s_last_state[128] = {};
static char s_last_large_text[128] = {};
static char s_last_small_text[128] = {};
static bool s_first_update = true;

static const char *status_name(PlayerStatus s) {
  switch (s) {
  case PlayerStatus::Driving:
    return "Driving";
  case PlayerStatus::Idle:
    return "Idle";
  case PlayerStatus::Parked:
    return "Parked";
  case PlayerStatus::Ferry:
    return "Ferry";
  case PlayerStatus::Train:
    return "Train";
  case PlayerStatus::InMenu:
    return "In Menu";
  default:
    return "Unknown";
  }
}

static void build_eta_str(char *buf, size_t size) {
  if (!g_job_active || g_nav_time <= 0.0f) {
    buf[0] = '\0';
    return;
  }
  int total_sec = static_cast<int>(g_nav_time);
  int hours = total_sec / 3600;
  int mins = (total_sec % 3600) / 60;
  if (hours > 0)
    snprintf(buf, size, "ETA %dh %dm", hours, mins);
  else
    snprintf(buf, size, "ETA %dm", mins);
}

static void build_activity(discord::Activity &activity) {
  activity.SetType(discord::ActivityType::Playing);

  // details
  char details[128];
  snprintf(details, sizeof(details), "%s", g_game_name);
  activity.SetDetails(details);

  // state
  char state[128];
  if (g_paused) {
    snprintf(state, sizeof(state), "In Menu");
  } else if (g_job_active && g_job_source_city[0] != '\0' &&
             g_job_dest_city[0] != '\0') {
    float dist_km = g_nav_distance / 1000.0f;
    if (dist_km >= 10.0f)
      snprintf(state, sizeof(state), "%s \u2192 %s  \u2022  %d km",
               g_job_source_city, g_job_dest_city,
               static_cast<int>(dist_km + 0.5f));
    else if (dist_km > 0.0f)
      snprintf(state, sizeof(state), "%s \u2192 %s  \u2022  %.1f km",
               g_job_source_city, g_job_dest_city, dist_km);
    else
      snprintf(state, sizeof(state), "%s \u2192 %s", g_job_source_city,
               g_job_dest_city);
  } else if (!g_job_active) {
    snprintf(state, sizeof(state), "Free Roam  \u2022  %s",
             status_name(g_player_status));
  } else {
    snprintf(state, sizeof(state), "%s", status_name(g_player_status));
  }
  if (g_multiplayer_offset != 0) {
    size_t room = sizeof(state) - strlen(state) - 1;
    strncat(state, "  \u2022  Convoy", room);
  }
  activity.SetState(state);

  if (g_session_start_realtime > 0)
    activity.GetTimestamps().SetStart(g_session_start_realtime);

  auto &assets = activity.GetAssets();
  const char *large_key = (strcmp(g_game_id, "ats") == 0)
                              ? DISCORD_LARGE_IMAGE_ATS
                              : DISCORD_LARGE_IMAGE_ETS2;
  assets.SetLargeImage(large_key);

  char large_text[128];
  build_eta_str(large_text, sizeof(large_text));
  assets.SetLargeText(large_text);

  const char *small_key =
      (g_truck_brand_id[0] != '\0' && has_truck_asset(g_truck_brand_id))
          ? g_truck_brand_id
          : DISCORD_SMALL_IMAGE_FALLBACK;
  assets.SetSmallImage(small_key);

  char small_text[64];
  if (g_truck_brand[0] != '\0' && g_truck_model[0] != '\0')
    snprintf(small_text, sizeof(small_text), "%s %s", g_truck_brand,
             g_truck_model);
  else if (g_truck_brand[0] != '\0')
    snprintf(small_text, sizeof(small_text), "%s", g_truck_brand);
  else
    snprintf(small_text, sizeof(small_text), "Truck");
  assets.SetSmallText(small_text);

  // Only call Discord SDK when something actually changed
  bool details_changed = strcmp(details, s_last_details) != 0;
  bool state_changed = strcmp(state, s_last_state) != 0;
  bool text_changed = strcmp(large_text, s_last_large_text) != 0;
  bool small_changed = strcmp(small_text, s_last_small_text) != 0;

  if (s_first_update || details_changed || state_changed || text_changed ||
      small_changed) {
    strncpy(s_last_details, details, sizeof(s_last_details));
    strncpy(s_last_state, state, sizeof(s_last_state));
    strncpy(s_last_large_text, large_text, sizeof(s_last_large_text));
    strncpy(s_last_small_text, small_text, sizeof(s_last_small_text));
    s_first_update = false;
    s_discord.update_activity(activity);
  }
}

void init_presence() { s_discord_inited = s_discord.init(DISCORD_APP_ID); }

void tick_presence() {
  static std::int64_t last_update = 0;
  std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));

  s_discord.run_callbacks();

  if (!s_discord.is_ready()) {
    if (s_discord_inited) {
      s_discord_inited = false;
      s_first_update = true;
    }
    return;
  }

  if (!s_discord_inited) {
    s_discord_inited = true;
    log_discord_reconnect();
  }

  // Throttle: 30s idle interval, 2s when dirty
  if (!g_presence_dirty && (now - last_update) < 30)
    return;
  if (g_presence_dirty && (now - last_update) < 2)
    return;
  g_presence_dirty = false;
  last_update = now;

  discord::Activity activity{};
  build_activity(activity);
}

void shutdown_presence() { s_discord.shutdown(); }
