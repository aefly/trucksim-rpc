#pragma once

#include "scssdk.h"
#include <cstdint>

enum class PlayerStatus {
  Unknown,
  InMenu,
  Driving,
  Idle,
  Parked,
  Ferry,
  Train
};

extern char g_game_name[64];
extern char g_game_id[16];
extern scs_u32_t g_game_version;

extern char g_truck_brand[64];
extern char g_truck_model[64];
extern char g_truck_brand_id[64];

extern char g_job_source_city[64];
extern char g_job_dest_city[64];
extern bool g_job_active;

extern float g_speed;
extern float g_nav_distance;
extern float g_nav_time;
extern scs_s32_t g_multiplayer_offset;
extern bool g_parking_brake;
extern bool g_engine_enabled;
extern scs_u32_t g_game_time;
extern bool g_paused;

extern PlayerStatus g_player_status;

extern std::int64_t g_session_start_realtime;

// Set true when presence needs an update, cleared after dispatch
extern bool g_presence_dirty;
