#include "telemetry.h"
#include "common/scssdk_telemetry_common_channels.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_common_gameplay_events.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "log.h"
#include "presence.h"
#include "scssdk_telemetry.h"
#include <cstring>
#include <ctime>

char g_game_name[64] = {};
char g_game_id[16] = {};
scs_u32_t g_game_version = 0;

char g_truck_brand[64] = {};
char g_truck_model[64] = {};
char g_truck_brand_id[64] = {};

char g_job_source_city[64] = {};
char g_job_dest_city[64] = {};
bool g_job_active = false;

float g_speed = 0.0f;
float g_nav_distance = 0.0f;
float g_nav_time = 0.0f;
scs_s32_t g_multiplayer_offset = 0;
bool g_parking_brake = false;
bool g_engine_enabled = false;
scs_u32_t g_game_time = 0;
bool g_paused = true;

PlayerStatus g_player_status = PlayerStatus::Unknown;

std::int64_t g_session_start_realtime = 0;

bool g_presence_dirty = false;

static bool g_on_ferry = false;
static bool g_on_train = false;

static void set_status(PlayerStatus s) {
  if (g_player_status == s)
    return;
  g_player_status = s;
  g_presence_dirty = true;
}

// Priority chain: pause > ferry/train > parked > driving > idle
static void infer_status() {
  if (g_paused) {
    set_status(PlayerStatus::InMenu);
    return;
  }
  if (g_on_ferry) {
    set_status(PlayerStatus::Ferry);
    return;
  }
  if (g_on_train) {
    set_status(PlayerStatus::Train);
    return;
  }
  if (!g_engine_enabled || g_parking_brake) {
    set_status(PlayerStatus::Parked);
    return;
  }
  if (g_speed > 0.5f) {
    set_status(PlayerStatus::Driving);
    return;
  }
  set_status(PlayerStatus::Idle);
}

// Channel callbacks

SCSAPI_VOID on_speed(const scs_string_t name, const scs_u32_t index,
                     const scs_value_t *const value,
                     const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_float)
    return;
  // SCS SDK may report negative speed in reverse
  float raw = value->value_float.value;
  if (raw < 0.0f)
    raw = -raw;
  g_speed = raw;
  infer_status();
}

SCSAPI_VOID on_parking_brake(const scs_string_t name, const scs_u32_t index,
                             const scs_value_t *const value,
                             const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_bool)
    return;
  g_parking_brake = value->value_bool.value != 0;
  infer_status();
}

SCSAPI_VOID on_engine_enabled(const scs_string_t name, const scs_u32_t index,
                              const scs_value_t *const value,
                              const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_bool)
    return;
  g_engine_enabled = value->value_bool.value != 0;
  infer_status();
}

SCSAPI_VOID on_nav_distance(const scs_string_t name, const scs_u32_t index,
                            const scs_value_t *const value,
                            const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_float)
    return;
  float raw = value->value_float.value;
  if (raw != g_nav_distance) {
    g_nav_distance = raw;
  }
}

SCSAPI_VOID on_nav_time(const scs_string_t name, const scs_u32_t index,
                        const scs_value_t *const value,
                        const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_float)
    return;
  float raw = value->value_float.value;
  if (raw != g_nav_time) {
    g_nav_time = raw;
  }
}

SCSAPI_VOID on_multiplayer_offset(const scs_string_t name,
                                  const scs_u32_t index,
                                  const scs_value_t *const value,
                                  const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_s32)
    return;
  scs_s32_t raw = value->value_s32.value;
  if (raw != g_multiplayer_offset) {
    g_multiplayer_offset = raw;
    g_presence_dirty = true;
  }
}

SCSAPI_VOID on_game_time(const scs_string_t name, const scs_u32_t index,
                         const scs_value_t *const value,
                         const scs_context_t context) {
  (void)name;
  (void)index;
  (void)context;
  if (!value || value->type != SCS_VALUE_TYPE_u32)
    return;
  scs_u32_t t = value->value_u32.value;
  if (t != g_game_time) {
    g_game_time = t;
    g_presence_dirty = true;
  }
}

// Event callbacks

// Handles both SCS_TELEMETRY_EVENT_paused and SCS_TELEMETRY_EVENT_started
SCSAPI_VOID on_pause(const scs_event_t event, const void *const event_info,
                     const scs_context_t context) {
  (void)event_info;
  (void)context;
  bool was = g_paused;
  g_paused = (event == SCS_TELEMETRY_EVENT_paused);
  if (was != g_paused) {
    infer_status();
  }
}

// Handles ferry/train transitions and job delivery/cancellation
SCSAPI_VOID on_gameplay(const scs_event_t event, const void *const event_info,
                        const scs_context_t context) {
  (void)event;
  (void)context;
  const scs_telemetry_gameplay_event_t *gpe =
      static_cast<const scs_telemetry_gameplay_event_t *>(event_info);
  if (!gpe || !gpe->id)
    return;

  if (strcmp(gpe->id, SCS_TELEMETRY_GAMEPLAY_EVENT_player_use_ferry) == 0) {
    g_on_ferry = true;
    g_on_train = false;
    infer_status();
  } else if (strcmp(gpe->id, SCS_TELEMETRY_GAMEPLAY_EVENT_player_use_train) ==
             0) {
    g_on_train = true;
    g_on_ferry = false;
    infer_status();
  } else if (strcmp(gpe->id, SCS_TELEMETRY_GAMEPLAY_EVENT_job_delivered) == 0 ||
             strcmp(gpe->id, SCS_TELEMETRY_GAMEPLAY_EVENT_job_cancelled) == 0) {
    g_job_active = false;
    g_job_source_city[0] = '\0';
    g_job_dest_city[0] = '\0';
    g_presence_dirty = true;
  }
}

SCSAPI_VOID on_config(const scs_event_t event, const void *const event_info,
                      const scs_context_t context) {
  (void)event;
  (void)context;
  const scs_telemetry_configuration_t *cfg =
      static_cast<const scs_telemetry_configuration_t *>(event_info);
  if (!cfg || !cfg->id || !cfg->attributes)
    return;

  if (strcmp(cfg->id, SCS_TELEMETRY_CONFIG_truck) == 0) {
    // brand_id is the Discord asset key; brand and name are display strings
    const scs_named_value_t *attr = cfg->attributes;
    while (attr->name) {
      if (strcmp(attr->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand_id) == 0 &&
          attr->value.type == SCS_VALUE_TYPE_string) {
        strncpy(g_truck_brand_id, attr->value.value_string.value,
                sizeof(g_truck_brand_id) - 1);
        g_truck_brand_id[sizeof(g_truck_brand_id) - 1] = '\0';
      } else if (strcmp(attr->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand) ==
                     0 &&
                 attr->value.type == SCS_VALUE_TYPE_string) {
        strncpy(g_truck_brand, attr->value.value_string.value,
                sizeof(g_truck_brand) - 1);
        g_truck_brand[sizeof(g_truck_brand) - 1] = '\0';
      } else if (strcmp(attr->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_name) == 0 &&
                 attr->value.type == SCS_VALUE_TYPE_string) {
        strncpy(g_truck_model, attr->value.value_string.value,
                sizeof(g_truck_model) - 1);
        g_truck_model[sizeof(g_truck_model) - 1] = '\0';
      }
      attr++;
    }
    g_presence_dirty = true;
  } else if (strcmp(cfg->id, SCS_TELEMETRY_CONFIG_job) == 0) {
    bool has_attrs = false;
    const scs_named_value_t *attr = cfg->attributes;
    while (attr->name) {
      has_attrs = true;
      if (strcmp(attr->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_city) == 0 &&
          attr->value.type == SCS_VALUE_TYPE_string) {
        strncpy(g_job_source_city, attr->value.value_string.value,
                sizeof(g_job_source_city) - 1);
        g_job_source_city[sizeof(g_job_source_city) - 1] = '\0';
      } else if (strcmp(attr->name,
                        SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_city) == 0 &&
                 attr->value.type == SCS_VALUE_TYPE_string) {
        strncpy(g_job_dest_city, attr->value.value_string.value,
                sizeof(g_job_dest_city) - 1);
        g_job_dest_city[sizeof(g_job_dest_city) - 1] = '\0';
      }
      attr++;
    }
    // Job config fires both at start (with attrs) and end (empty attrs)
    if (has_attrs && g_job_source_city[0] != '\0') {
      g_job_active = true;
      g_on_ferry = false;
      g_on_train = false;

    } else {
      g_job_active = false;
      g_job_source_city[0] = '\0';
      g_job_dest_city[0] = '\0';
    }
    g_presence_dirty = true;
  }
}

SCSAPI_VOID on_frame_start(const scs_event_t event,
                           const void *const event_info,
                           const scs_context_t context) {
  (void)event;
  (void)context;
  const scs_telemetry_frame_start_t *info =
      static_cast<const scs_telemetry_frame_start_t *>(event_info);
  if (!info)
    return;

  if (info->flags & SCS_TELEMETRY_FRAME_START_FLAG_timer_restart) {
    g_session_start_realtime = static_cast<std::int64_t>(std::time(nullptr));
    g_presence_dirty = true;
  }

  // Clear ferry/train when speed detected driving
  if (g_speed > 0.5f && (g_on_ferry || g_on_train)) {
    g_on_ferry = false;
    g_on_train = false;
    infer_status();
  }

  tick_presence();
}

static void copy_string(char *dst, size_t dst_size, scs_string_t src) {
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

SCSAPI_RESULT
scs_telemetry_init(const scs_u32_t version,
                   const scs_telemetry_init_params_t *const params) {
  const scs_telemetry_init_params_v100_t *p =
      static_cast<const scs_telemetry_init_params_v100_t *>(params);

  log_init(p->common.log);
  log_init_start();

  copy_string(g_game_name, sizeof(g_game_name), p->common.game_name);
  copy_string(g_game_id, sizeof(g_game_id), p->common.game_id);
  g_game_version = p->common.game_version;

  log_game_info(g_game_name, g_game_version);

  g_session_start_realtime = static_cast<std::int64_t>(std::time(nullptr));
  g_paused = true; // game starts in paused state
  g_player_status = PlayerStatus::InMenu;

  p->register_for_event(SCS_TELEMETRY_EVENT_frame_start, on_frame_start,
                        nullptr);

  p->register_for_event(SCS_TELEMETRY_EVENT_paused, on_pause, nullptr);

  p->register_for_event(SCS_TELEMETRY_EVENT_started, on_pause, nullptr);

  p->register_for_event(SCS_TELEMETRY_EVENT_configuration, on_config, nullptr);

  p->register_for_event(SCS_TELEMETRY_EVENT_gameplay, on_gameplay, nullptr);

  p->register_for_channel(
      SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float,
      SCS_TELEMETRY_CHANNEL_FLAG_each_frame, on_speed, nullptr);

  p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake,
                          SCS_U32_NIL, SCS_VALUE_TYPE_bool,
                          SCS_TELEMETRY_CHANNEL_FLAG_each_frame,
                          on_parking_brake, nullptr);

  p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled,
                          SCS_U32_NIL, SCS_VALUE_TYPE_bool,
                          SCS_TELEMETRY_CHANNEL_FLAG_each_frame,
                          on_engine_enabled, nullptr);

  p->register_for_channel(SCS_TELEMETRY_CHANNEL_game_time, SCS_U32_NIL,
                          SCS_VALUE_TYPE_u32, SCS_TELEMETRY_CHANNEL_FLAG_none,
                          on_game_time, nullptr);

  p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance,
                          SCS_U32_NIL, SCS_VALUE_TYPE_float,
                          SCS_TELEMETRY_CHANNEL_FLAG_none, on_nav_distance,
                          nullptr);

  p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time,
                          SCS_U32_NIL, SCS_VALUE_TYPE_float,
                          SCS_TELEMETRY_CHANNEL_FLAG_none, on_nav_time,
                          nullptr);

  p->register_for_channel(SCS_TELEMETRY_CHANNEL_multiplayer_time_offset,
                          SCS_U32_NIL, SCS_VALUE_TYPE_s32,
                          SCS_TELEMETRY_CHANNEL_FLAG_none,
                          on_multiplayer_offset, nullptr);

  g_presence_dirty = true;

  init_presence();

  log_init_complete();
  return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void) { shutdown_presence(); }
