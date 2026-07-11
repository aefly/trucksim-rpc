#pragma once

#include <cstddef>
#include <cstring>

// APP ID — define via -DDISCORD_APP_ID=<id> at build time.
// Local: set APP_ID=<id> in .env (gitignored).
// CI: set APP_ID secret in GitHub repo settings.
#ifndef DISCORD_APP_ID
#error "DISCORD_APP_ID not defined. Set APP_ID=<id> in .env and run build.sh"
#endif

// Rich Presence Assets
constexpr const char *DISCORD_LARGE_IMAGE_ETS2 = "ets2";
constexpr const char *DISCORD_LARGE_IMAGE_ATS = "ats";
constexpr const char *DISCORD_SMALL_IMAGE_FALLBACK = "steering";

// Asset keys registered in Discord Developer Portal.
constexpr const char *TRUCK_BRAND_ASSETS[] = {
    "volvo",       "scania",      "mercedes", "renault",   "iveco",
    "man",         "daf",         "kenworth", "peterbilt", "freightliner",
    "westernstar", "intnational", "mack"};
constexpr size_t TRUCK_BRAND_ASSETS_COUNT =
    sizeof(TRUCK_BRAND_ASSETS) / sizeof(TRUCK_BRAND_ASSETS[0]);

static inline bool has_truck_asset(const char *id) {
  for (size_t i = 0; i < TRUCK_BRAND_ASSETS_COUNT; i++)
    if (strcmp(id, TRUCK_BRAND_ASSETS[i]) == 0)
      return true;
  return false;
}
